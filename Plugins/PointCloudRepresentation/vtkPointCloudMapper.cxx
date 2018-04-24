/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPointsCloudMapper.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPointCloudMapper.h"

#include <vtkActor.h>
#include <vtkBoundingBox.h>
#include <vtkExecutive.h>
#include <vtkImageData.h>
#include <vtkInformation.h>
#include <vtkMath.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLActor.h>
#include <vtkOpenGLBufferObject.h>
#include <vtkOpenGLCamera.h>
#include <vtkOpenGLError.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkOpenGLShaderCache.h>
#include <vtkOpenGLTexture.h>
#include <vtkOpenGLVertexArrayObject.h>
#include <vtkPointData.h>
#include <vtkPointSet.h>
#include <vtkPoints.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkScalarsToColors.h>
#include <vtkShader.h>
#include <vtkShaderProgram.h>

#include <cassert>

extern const char* vtkPointCloud_fs;
extern const char* vtkPointCloud_vs;

//-----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPointCloudMapper);

//-----------------------------------------------------------------------------
vtkPointCloudMapper::vtkPointCloudMapper()
{
  this->MaxNumberOfPoints = 0;

  this->VBOPos = vtkSmartPointer<vtkOpenGLBufferObject>::New() ;
  this->VBOData = vtkSmartPointer<vtkOpenGLBufferObject>::New() ;
  this->VAO = vtkSmartPointer<vtkOpenGLVertexArrayObject>::New() ;

  vtkMath::UninitializeBounds(this->Bounds);
  this->SetInputArrayToProcess(
    0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, vtkDataSetAttributes::SCALARS);
}

//-----------------------------------------------------------------------------
vtkPointCloudMapper::~vtkPointCloudMapper()
{
}

//-----------------------------------------------------------------------------
void vtkPointCloudMapper::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//-----------------------------------------------------------------------------
vtkDataArray* vtkPointCloudMapper::GetDataArray(vtkPointSet* ps)
{
  vtkDataArray* data = nullptr;
  if (this->GetScalarVisibility())
  {
    data = ps->GetPointData()->GetArray(this->GetArrayName());
    if (!data)
    {
      data = this->GetInputArrayToProcess(0, 0, this->GetExecutive()->GetInputInformation());
    }
  }
  if (data && data->GetNumberOfComponents() > 1)
  {
    vtkWarningMacro(<< "This mapper only support rendering of data arrays with 1 component!");
    data = nullptr;
  }
  return data;
}

//-----------------------------------------------------------------------------
void vtkPointCloudMapper::SetupShaders(vtkRenderer* ren)
{
  // Setup shaders
  vtkOpenGLRenderWindow* renWin = vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow());
  assert(renWin);
  if (this->Program.Get() == nullptr)
  {
    std::map<vtkShader::Type, vtkShader*> shaders;

    vtkNew<vtkShader> vss;
    vss->SetType(vtkShader::Vertex);
    vss->SetSource(vtkPointCloud_vs);
    shaders[vtkShader::Vertex] = vss.Get();

    vtkNew<vtkShader> gss;
    gss->SetType(vtkShader::Geometry);
    shaders[vtkShader::Geometry] = gss.Get();

    vtkNew<vtkShader> fss;
    fss->SetType(vtkShader::Fragment);
    fss->SetSource(vtkPointCloud_fs);
    shaders[vtkShader::Fragment] = fss.Get();

    // Build and bind shader program
    this->Program = renWin->GetShaderCache()->ReadyShaderProgram(shaders);
  }
  else
  {
    // Bind the shader program
    renWin->GetShaderCache()->ReadyShaderProgram(this->Program);
  }
}

//-----------------------------------------------------------------------------
void vtkPointCloudMapper::SetupMatrix(vtkRenderer* ren, vtkActor* actor)
{
  // Prepare MCDC transform matrix
  vtkOpenGLCamera* cam = vtkOpenGLCamera::SafeDownCast(ren->GetActiveCamera());
  assert(cam);

  vtkMatrix3x3* norms;
  vtkMatrix4x4 *wcdc, *wcvc, *vcdc;
  cam->GetKeyMatrices(ren, wcvc, norms, vcdc, wcdc);

  actor->ComputeMatrix();
  if (!actor->GetIsIdentity())
  {
    vtkMatrix4x4* mcwc;
    vtkMatrix3x3* anorms;
    vtkNew<vtkMatrix4x4> tempMatrix4;
    static_cast<vtkOpenGLActor*>(actor)->GetKeyMatrices(mcwc, anorms);
    vtkMatrix4x4::Multiply4x4(mcwc, wcdc, tempMatrix4.Get());
    this->Program->SetUniformMatrix("MCDCMatrix", tempMatrix4.Get());
  }
  else
  {
    this->Program->SetUniformMatrix("MCDCMatrix", wcdc);
  }
}

//-----------------------------------------------------------------------------
void vtkPointCloudMapper::SetupVBOs(vtkPoints* points, vtkDataArray* data)
{
  vtkIdType numPts = points->GetNumberOfPoints();

  // Generate VBO if it does not exist yet
  this->VBOPos->GenerateBuffer(vtkOpenGLBufferObject::ArrayBuffer);
  this->VBOData->GenerateBuffer(vtkOpenGLBufferObject::ArrayBuffer);

  bool bufferRefresh = false;

  if (numPts > this->MaxNumberOfPoints)
  {
    // We need to increase buffers - we allocated 20% of points more than
    // current number of points to avoid reallocation if number of points
    // increase a bit in next time step.
    this->MaxNumberOfPoints = static_cast<vtkIdType>(numPts * 1.2);

    this->VBOPos->Bind();
    glBufferData(
      GL_ARRAY_BUFFER, 3 * this->MaxNumberOfPoints * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    this->VAO->AddAttributeArray(this->Program.Get(),
      this->VBOPos.Get(),
      "vertexWC",
      0,
      3 * sizeof(float),
      VTK_FLOAT,
      3,
      false);
    this->VBOPos->Release();

    this->VBOData->Bind();
    glBufferData(
      GL_ARRAY_BUFFER, this->MaxNumberOfPoints * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    this->VAO->AddAttributeArray(this->Program.Get(),
      this->VBOData.Get(),
      "vertexData",
      0,
      sizeof(float),
      VTK_FLOAT,
      1,
      false);
    this->VBOPos->Release();

    bufferRefresh = true;
  }

  if (points->GetMTime() > this->VBOPos->GetMTime() || bufferRefresh)
  {
    // copy points to mapped buffer
    this->VBOPos->Bind();
    void* ptsBuffer = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
    memcpy(ptsBuffer, points->GetVoidPointer(0), 3 * numPts * sizeof(float));
    glUnmapBuffer(GL_ARRAY_BUFFER);
    this->VBOPos->Release();
    this->VBOPos->Modified();
  }

  if (data && (data->GetMTime() > this->VBOData->GetMTime() || bufferRefresh))
  {
    std::vector<float> convertedData(numPts);
    for (int i = 0; i < numPts; i++)
    {
      switch (data->GetDataType())
      {
        case VTK_FLOAT:
          convertedData[i] = reinterpret_cast<float*>(data->GetVoidPointer(0))[i];
          break;
        case VTK_DOUBLE:
          convertedData[i] =
            static_cast<float>(reinterpret_cast<double*>(data->GetVoidPointer(0))[i]);
          break;
        case VTK_UNSIGNED_CHAR:
          convertedData[i] =
            static_cast<float>(reinterpret_cast<unsigned char*>(data->GetVoidPointer(0))[i]);
          break;
        case VTK_INT:
          convertedData[i] = static_cast<float>(reinterpret_cast<int*>(data->GetVoidPointer(0))[i]);
          break;
        case VTK_UNSIGNED_INT:
          convertedData[i] =
            static_cast<float>(reinterpret_cast<unsigned int*>(data->GetVoidPointer(0))[i]);
          break;
      }
    }
    this->VBOData->Bind();
    void* dataBuffer = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE);
    memcpy(dataBuffer, convertedData.data(), numPts * sizeof(float));
    glUnmapBuffer(GL_ARRAY_BUFFER);
    this->VBOData->Release();
    this->VBOData->Modified();
  }
}

//-----------------------------------------------------------------------------
void vtkPointCloudMapper::SetupColors(vtkRenderer* ren, vtkActor* actor, vtkDataArray* data)
{
  // Create color texture
  if (data && ((this->InternalColorTexture.Get() == nullptr) ||
                this->LookupTable->GetMTime() > this->InternalColorTexture->GetMTime()))
  {
    this->MapScalarsToTexture(data, 1.0);

    float range[2];
    // range[0] = this->LookupTable->GetRange()[0];
    // range[1] = this->LookupTable->GetRange()[1];

    // force range to intensity (0-255) but we have to open it
    range[0] = 0.0;
    range[1] = 255.0;

    this->Program->SetUniform2f("dataRange", range);

    this->InternalColorTexture = vtkSmartPointer<vtkOpenGLTexture>::New();
    this->InternalColorTexture->RepeatOff();
    this->InternalColorTexture->SetInputData(this->ColorTextureMap);
    this->InternalColorTexture->Modified();
  }

  if (this->InternalColorTexture.Get() != nullptr)
  {
    this->InternalColorTexture->Render(ren);
    this->Program->SetUniformi("colorTex", this->InternalColorTexture->GetTextureUnit());
  }

  // Setup color & opacity
  this->Program->SetUniformi("hasVertexData", data != nullptr ? 1 : 0);
  float solidColor[4] = { 1.f, 1.f, 1.f, 1.f };
  if (data == nullptr)
  {
    const double* color = actor->GetProperty()->GetColor();
    for (int i = 0; i < 3; i++)
    {
      solidColor[i] = color[i];
    }
  }
  this->Program->SetUniform4f("solidColor", solidColor);
  this->Program->SetUniformf("opacity", actor->GetProperty()->GetOpacity());
}

//-----------------------------------------------------------------------------
void vtkPointCloudMapper::Render(vtkRenderer* ren, vtkActor* actor)
{
  vtkPointSet* ps = this->GetInput();
  vtkPoints* points = ps ? ps->GetPoints() : nullptr;
  vtkIdType numPts = points ? points->GetNumberOfPoints() : 0;
  if (numPts == 0)
  {
    return;
  }

  if (points->GetDataType() != VTK_FLOAT)
  {
    vtkWarningMacro(<< "This mapper only support rendering of float point sets!");
    return;
  }

  // Fetch color data array
  vtkDataArray* data = this->GetDataArray(ps);

  this->SetupShaders(ren);

  this->SetupMatrix(ren, actor);

  this->SetupColors(ren, actor, data);

  // Bind VAO & setup VBOs
  this->VAO->Bind();

  this->SetupVBOs(points, data);

#if GL_ES_VERSION_3_0 != 1
  glPointSize(actor->GetProperty()->GetPointSize());
#endif

  // Finally draw the point cloud!
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(numPts));

  if (this->InternalColorTexture.Get() != nullptr)
  {
    this->InternalColorTexture->PostRender(ren);
  }
  this->VAO->Release();
}

//----------------------------------------------------------------------------
void vtkPointCloudMapper::SetInputData(vtkPointSet* input)
{
  if (input != this->GetInput())
  {
    this->SetInputDataInternal(0, input);
    if (!vtkBoundingBox::IsValid(this->Bounds))
    {
      this->SetBounds(input->GetBounds());
    }
  }
}

//----------------------------------------------------------------------------
vtkPointSet* vtkPointCloudMapper::GetInput()
{
  return vtkPointSet::SafeDownCast(this->GetExecutive()->GetInputData(0, 0));
}

//----------------------------------------------------------------------------
int vtkPointCloudMapper::FillInputPortInformation(int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkPointSet");
  return 1;
}
