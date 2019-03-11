/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkPointCloudRepresentation.h"

#include <vtkActor.h>
#include <vtkAlgorithmOutput.h>
#include <vtkCellData.h>
#include <vtkColorTransferFunction.h>
#include <vtkCommand.h>
#include <vtkCompositeDataSet.h>
#include <vtkCompositeDataToUnstructuredGridFilter.h>
#include <vtkExtentTranslator.h>
#include <vtkImageData.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkMaskPoints.h>
#include <vtkMath.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPExtentTranslator.h>
#include <vtkPVRenderView.h>
#include <vtkPointCloudMapper.h>
#include <vtkPointSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkStreamingDemandDrivenPipeline.h>
#include <vtkStructuredData.h>
#include <vtkTransform.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnstructuredGrid.h>

#include <algorithm>
#include <map>
#include <string>

vtkStandardNewMacro(vtkPointCloudRepresentation);

//----------------------------------------------------------------------------
vtkPointCloudRepresentation::vtkPointCloudRepresentation()
{
  this->Mapper = vtkSmartPointer<vtkPointCloudMapper>::New();

  this->Property = vtkSmartPointer<vtkProperty>::New();

  this->Actor = vtkSmartPointer<vtkActor>::New();
  this->Actor->SetProperty(this->Property);

  vtkMath::UninitializeBounds(this->Bounds);
}

//----------------------------------------------------------------------------
vtkPointCloudRepresentation::~vtkPointCloudRepresentation()
{
}

//----------------------------------------------------------------------------
int vtkPointCloudRepresentation::FillInputPortInformation(int, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkDataSet");
  info->Append(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkMultiBlockDataSet");

  // Saying INPUT_IS_OPTIONAL() is essential, since representations don't have
  // any inputs on client-side (in client-server, client-render-server mode) and
  // render-server-side (in client-render-server mode).
  info->Set(vtkAlgorithm::INPUT_IS_OPTIONAL(), 1);

  return 1;
}

//----------------------------------------------------------------------------
int vtkPointCloudRepresentation::ProcessViewRequest(vtkInformationRequestKey* request_type,
  vtkInformation* inInfo,
  vtkInformation* outInfo)
{
  // always forward to superclass first. Superclass returns 0 if the
  // representation is not visible (among other things). In which case there's
  // nothing to do.
  if (!this->Superclass::ProcessViewRequest(request_type, inInfo, outInfo))
  {
    return 0;
  }

  if (request_type == vtkPVView::REQUEST_UPDATE())
  {
    double bounds[6] = { 0, 0, 0, 0, 0, 0 };
    // Standard representation stuff, first.
    // 1. Provide the data being rendered.
    if (this->ProcessedData)
    {
      vtkPVRenderView::SetPiece(inInfo, this, this->ProcessedData);
      this->ProcessedData->GetBounds(bounds);
    }

    // 2. Provide the bounds.
    vtkNew<vtkMatrix4x4> matrix;
    this->Actor->GetMatrix(matrix.GetPointer());
    vtkPVRenderView::SetGeometryBounds(inInfo, bounds, matrix.GetPointer());
    outInfo->Set(vtkPVRenderView::NEED_ORDERED_COMPOSITING(), 1);
  }
  else if (request_type == vtkPVView::REQUEST_RENDER())
  {
    vtkAlgorithmOutput* producerPort = vtkPVRenderView::GetPieceProducer(inInfo, this);
    this->Mapper->SetInputConnection(producerPort);
    this->UpdateMapperParameters();
  }

  this->MarkModified();

  return 1;
}

//----------------------------------------------------------------------------
int vtkPointCloudRepresentation::RequestData(vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  vtkSmartPointer<vtkDataSet> input = vtkDataSet::GetData(inputVector[0]);
  vtkPointSet* inputPS = vtkPointSet::SafeDownCast(input);
  vtkCompositeDataSet* compositeInput = vtkCompositeDataSet::GetData(inputVector[0], 0);
  auto* multiBlockInput = vtkMultiBlockDataSet::GetData(inputVector[0]);
  this->ProcessedData = nullptr;
  if (inputPS)
  {
    this->ProcessedData = inputPS;
  }
  else if (multiBlockInput)
  {
    auto pointSet = vtkSmartPointer<vtkPolyData>::New();
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(multiBlockInput->GetNumberOfPoints());
    float* destination = reinterpret_cast<float*>(points->GetData()->GetVoidPointer(0));
    for (int i = 0; i < multiBlockInput->GetNumberOfBlocks(); ++i)
    {
      vtkPolyData* poly = vtkPolyData::SafeDownCast(multiBlockInput->GetBlock(i));
      if (poly)
      {
        int nbPoint = poly->GetNumberOfPoints();

        float*  source = reinterpret_cast<float*>(poly->GetPoints()->GetData()->GetVoidPointer(0));
        memcpy(destination, source, 3 * nbPoint * sizeof(float));
        destination += 3 * nbPoint;
      }
    }

    pointSet->SetPoints(points.Get());
    this->ProcessedData = pointSet;
  }
  else if (compositeInput)
  {
    vtkNew<vtkCompositeDataToUnstructuredGridFilter> merge;
    merge->SetInputData(compositeInput);
    merge->Update();
    input = merge->GetOutput();
  }

  // The mapper underneath expect only PolyData
  // Apply conversion - We do not need vertex list as we
  // use all the points in that use case
  if (this->ProcessedData == nullptr && input != nullptr && input->GetNumberOfPoints() > 0)
  {
    vtkNew<vtkMaskPoints> unstructuredToPolyData;
    unstructuredToPolyData->SetInputData(input);
    unstructuredToPolyData->SetMaximumNumberOfPoints(input->GetNumberOfPoints());
    unstructuredToPolyData->GenerateVerticesOff();
    unstructuredToPolyData->SetOnRatio(1);
    unstructuredToPolyData->Update();
    this->ProcessedData = unstructuredToPolyData->GetOutput();
  }

  if (this->ProcessedData == nullptr)
  {
    this->ProcessedData = vtkSmartPointer<vtkPolyData>::New();
  }

  return this->Superclass::RequestData(request, inputVector, outputVector);
}

//----------------------------------------------------------------------------
bool vtkPointCloudRepresentation::AddToView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->AddActor(this->Actor);
    return this->Superclass::AddToView(view);
  }
  return false;
}

//----------------------------------------------------------------------------
bool vtkPointCloudRepresentation::RemoveFromView(vtkView* view)
{
  vtkPVRenderView* rview = vtkPVRenderView::SafeDownCast(view);
  if (rview)
  {
    rview->GetRenderer()->RemoveActor(this->Actor);
    return this->Superclass::RemoveFromView(view);
  }
  return false;
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::UpdateMapperParameters()
{
  this->Actor->SetMapper(this->Mapper);
  this->Actor->SetVisibility(1);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
const char* vtkPointCloudRepresentation::GetColorArrayName()
{
  vtkInformation* info = this->GetInputArrayInformation(0);
  if (info && info->Has(vtkDataObject::FIELD_ASSOCIATION()) &&
    info->Has(vtkDataObject::FIELD_NAME()))
  {
    return info->Get(vtkDataObject::FIELD_NAME());
  }
  return NULL;
}

//----------------------------------------------------------------------------
// Forwarded to Actor.
//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetColor(double r, double g, double b)
{
  this->Property->SetColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetLineWidth(double val)
{
  this->Property->SetLineWidth(val);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetOpacity(double val)
{
  this->Property->SetOpacity(val);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetPointSize(double val)
{
  this->Property->SetPointSize(val);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetAmbientColor(double r, double g, double b)
{
  this->Property->SetAmbientColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetDiffuseColor(double r, double g, double b)
{
  this->Property->SetDiffuseColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetEdgeColor(double r, double g, double b)
{
  this->Property->SetEdgeColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetInterpolation(int val)
{
  this->Property->SetInterpolation(val);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetSpecularColor(double r, double g, double b)
{
  this->Property->SetSpecularColor(r, g, b);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetSpecularPower(double val)
{
  this->Property->SetSpecularPower(val);
}

//***************************************************************************
// Forwarded to Actor.
//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetVisibility(bool val)
{
  this->Superclass::SetVisibility(val);
  this->Actor->SetVisibility(val ? 1 : 0);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetOrientation(double x, double y, double z)
{
  this->Actor->SetOrientation(x, y, z);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetOrigin(double x, double y, double z)
{
  this->Actor->SetOrigin(x, y, z);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetPickable(int val)
{
  this->Actor->SetPickable(val);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetPosition(double x, double y, double z)
{
  this->Actor->SetPosition(x, y, z);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetScale(double x, double y, double z)
{
  this->Actor->SetScale(x, y, z);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetUserTransform(const double matrix[16])
{
  vtkNew<vtkTransform> transform;
  transform->SetMatrix(matrix);
  this->Actor->SetUserTransform(transform.GetPointer());
}

//****************************************************************************
// Methods merely forwarding parameters to internal objects.
//****************************************************************************

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetLookupTable(vtkScalarsToColors* val)
{
  this->Mapper->SetLookupTable(val);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetMapScalars(int val)
{
  if (val < 0 || val > 1)
  {
    vtkWarningMacro(<< "Invalid parameter for vtkPointCloudRepresentation::SetMapScalars: " << val);
    val = 0;
  }
  int mapToColorMode[] = { VTK_COLOR_MODE_DIRECT_SCALARS, VTK_COLOR_MODE_MAP_SCALARS };
  this->Mapper->SetColorMode(mapToColorMode[val]);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetInterpolateScalarsBeforeMapping(int val)
{
  this->Mapper->SetInterpolateScalarsBeforeMapping(val);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetColorArrayName(const char* name)
{
  this->SetInputArrayToProcess(0, 0, 0, vtkDataObject::FIELD_ASSOCIATION_POINTS, name);
}

//----------------------------------------------------------------------------
void vtkPointCloudRepresentation::SetInputArrayToProcess(int idx,
  int port,
  int connection,
  int fieldAssociation,
  const char* name)
{
  this->Superclass::SetInputArrayToProcess(idx, port, connection, fieldAssociation, name);

  if (idx == 1)
  {
    return;
  }

  this->Mapper->SetInputArrayToProcess(idx, port, connection, fieldAssociation, name);

  if (name && name[0])
  {
    this->Mapper->SetScalarVisibility(1);
    this->Mapper->SelectColorArray(name);
  }
  else
  {
    this->Mapper->SetScalarVisibility(0);
    this->Mapper->SelectColorArray(static_cast<const char*>(NULL));
  }

  switch (fieldAssociation)
  {
    case vtkDataObject::FIELD_ASSOCIATION_CELLS:
      this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_CELL_FIELD_DATA);
      break;

    case vtkDataObject::FIELD_ASSOCIATION_POINTS:
    default:
      this->Mapper->SetScalarMode(VTK_SCALAR_MODE_USE_POINT_FIELD_DATA);
      break;
  }
}
