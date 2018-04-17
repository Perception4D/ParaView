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
/**
 * @class   vtkPointCloudRepresentation
 * @brief   Representation for displaying point clouds with adequate mapper.
 *
 * @par Thanks:
 * This class was written by Joachim Pouderoux, Kitware 2018
 */

#ifndef vtkPointCloudRepresentation_h
#define vtkPointCloudRepresentation_h

#include <vtkPVDataRepresentation.h>
#include <vtkSmartPointer.h> // needed for vtkSmartPointer.

class vtkActor;
class vtkColorTransferFunction;
class vtkInformation;
class vtkInformationRequestKey;
class vtkPointSet;
class vtkPointCloudMapper;
class vtkProperty;
class vtkScalarsToColors;

class vtkPointCloudRepresentation : public vtkPVDataRepresentation
{
public:
  static vtkPointCloudRepresentation* New();
  vtkTypeMacro(vtkPointCloudRepresentation, vtkPVDataRepresentation);
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  vtkSetVectorMacro(Bounds, double, 6);

  /**
   * vtkAlgorithm::ProcessRequest() equivalent for rendering passes. This is
   * typically called by the vtkView to request meta-data from the
   * representations or ask them to perform certain tasks e.g.
   * PrepareForRendering.
   */
  int ProcessViewRequest(vtkInformationRequestKey*, vtkInformation*, vtkInformation*) VTK_OVERRIDE;

  /**
   * Get/Set the visibility for this representation. When the visibility of
   * representation of false, all view passes are ignored.
   */
  void SetVisibility(bool val) VTK_OVERRIDE;

  //***************************************************************************
  // Forwarded to vtkProperty.
  virtual void SetAmbientColor(double r, double g, double b);
  virtual void SetColor(double r, double g, double b);
  virtual void SetDiffuseColor(double r, double g, double b);
  virtual void SetEdgeColor(double r, double g, double b);
  virtual void SetInterpolation(int val);
  virtual void SetLineWidth(double val);
  virtual void SetOpacity(double val);
  virtual void SetPointSize(double val);
  virtual void SetSpecularColor(double r, double g, double b);
  virtual void SetSpecularPower(double val);

  //***************************************************************************
  // Forwarded to Actor.
  virtual void SetOrientation(double, double, double);
  virtual void SetOrigin(double, double, double);
  virtual void SetPickable(int val);
  virtual void SetPosition(double, double, double);
  virtual void SetScale(double, double, double);
  virtual void SetUserTransform(const double[16]);

  //***************************************************************************
  // Forwarded to Mapper and LODMapper.
  virtual void SetInterpolateScalarsBeforeMapping(int val);
  virtual void SetLookupTable(vtkScalarsToColors* val);

  //@{
  /**
   * Sets if scalars are mapped through a color-map or are used
   * directly as colors.
   * 0 maps to VTK_COLOR_MODE_DIRECT_SCALARS
   * 1 maps to VTK_COLOR_MODE_MAP_SCALARS
   * @see vtkScalarsToColors::MapScalars
   */
  void SetMapScalars(int val);
  //@}

  /**
   * Fill input port information.
   */
  int FillInputPortInformation(int port, vtkInformation* info) VTK_OVERRIDE;

  /**
   * Convenience method to get the array name used to scalar color with.
   */
  const char* GetColorArrayName();

  /**
   * Convenience method to set the array name used to scalar color with.
   */
  void SetColorArrayName(const char* name);

  // Description:
  // Set the input data arrays that this algorithm will process. Overridden to
  // pass the array selection to the mapper.
  void SetInputArrayToProcess(int idx,
    int port,
    int connection,
    int fieldAssociation,
    const char* name) VTK_OVERRIDE;
  void SetInputArrayToProcess(int idx,
    int port,
    int connection,
    int fieldAssociation,
    int fieldAttributeType) VTK_OVERRIDE
  {
    this->Superclass::SetInputArrayToProcess(
      idx, port, connection, fieldAssociation, fieldAttributeType);
  }
  void SetInputArrayToProcess(int idx, vtkInformation* info) VTK_OVERRIDE
  {
    this->Superclass::SetInputArrayToProcess(idx, info);
  }
  void SetInputArrayToProcess(int idx,
    int port,
    int connection,
    const char* fieldAssociation,
    const char* attributeTypeorName) VTK_OVERRIDE
  {
    this->Superclass::SetInputArrayToProcess(
      idx, port, connection, fieldAssociation, attributeTypeorName);
  }

protected:
  vtkPointCloudRepresentation();
  ~vtkPointCloudRepresentation() override;

  int RequestData(vtkInformation*, vtkInformationVector**, vtkInformationVector*) VTK_OVERRIDE;

  /**
   * Adds the representation to the view.  This is called from
   * vtkView::AddRepresentation().  Subclasses should override this method.
   * Returns true if the addition succeeds.
   */
  bool AddToView(vtkView* view) VTK_OVERRIDE;

  /**
   * Removes the representation to the view.  This is called from
   * vtkView::RemoveRepresentation().  Subclasses should override this method.
   * Returns true if the removal succeeds.
   */
  bool RemoveFromView(vtkView* view) VTK_OVERRIDE;

  /**
   * Passes on parameters to the active mapper
   */
  void UpdateMapperParameters();

  vtkSmartPointer<vtkPointSet> ProcessedData;
  vtkSmartPointer<vtkPointCloudMapper> Mapper;
  vtkSmartPointer<vtkProperty> Property;
  vtkSmartPointer<vtkActor> Actor;

  double Bounds[6];

private:
  vtkPointCloudRepresentation(const vtkPointCloudRepresentation&); // Not implemented
  void operator=(const vtkPointCloudRepresentation&); // Not implemented
};

#endif
