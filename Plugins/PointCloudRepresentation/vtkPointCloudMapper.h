/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkPointsCloudMapper.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkPointsCloudMapper
 * @brief   Mapper for effective point cloud visualization.
 *
 */

#ifndef vtkPointsCloudMapper_h
#define vtkPointsCloudMapper_h

#include "vtkMapper.h"
#include "vtkNew.h"

class vtkDataArray;
class vtkOpenGLBufferObject;
class vtkOpenGLTexture;
class vtkOpenGLVertexArrayObject;
class vtkPoints;
class vtkPointSet;
class vtkShaderProgram;

class VTK_EXPORT vtkPointCloudMapper : public vtkMapper
{
public:
  static vtkPointCloudMapper* New();
  vtkTypeMacro(vtkPointCloudMapper, vtkMapper);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  void Render(vtkRenderer* ren, vtkActor* actor) override;

  void SetInputData(vtkPointSet* in);
  vtkPointSet* GetInput();

  vtkSetVectorMacro(Bounds, double, 6);

protected:
  vtkPointCloudMapper();
  ~vtkPointCloudMapper();

  int FillInputPortInformation(int port, vtkInformation* info) override;

  vtkDataArray* GetDataArray(vtkPointSet*);
  void SetupShaders(vtkRenderer*);
  void SetupMatrix(vtkRenderer*, vtkActor*);
  void SetupColors(vtkRenderer*, vtkActor*, vtkDataArray*);
  void SetupVBOs(vtkPoints*, vtkDataArray*);

  vtkSmartPointer<vtkShaderProgram> Program;

  vtkSmartPointer<vtkOpenGLBufferObject> VBOPos;
  vtkSmartPointer<vtkOpenGLBufferObject> VBOData;
  vtkSmartPointer<vtkOpenGLVertexArrayObject> VAO;

  vtkSmartPointer<vtkOpenGLTexture> InternalColorTexture;

  vtkIdType MaxNumberOfPoints;

private:
  vtkPointCloudMapper(const vtkPointCloudMapper&) ; // Not implemented
  void operator=(const vtkPointCloudMapper&) ; // Not implemented
};

#endif
