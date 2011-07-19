/*=========================================================================

Program: ParaView
Module:    PrismScaleViewDialog.h

=========================================================================*/
#ifndef __PrismScaleViewDialog_h 
#define __PrismScaleViewDialog_h

#include <QDialog>
#include <QString>

class PrismView;
class QAbstractButton;
class PrismScaleViewDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;
public:
  PrismScaleViewDialog(QWidget* parent=0, Qt::WindowFlags flags=0);
  virtual ~PrismScaleViewDialog();
  
  void setView(PrismView* view);
protected slots:
  void onModeChanged(const QString& mode);
  void onCustomBoundsChanged();
  void onButtonClicked(QAbstractButton * button);

protected:
  bool hasCustomBounds() const;
  void modeChanged(const int& pos, const int& value);
  void setupViewInfo();
  void updateView();
  
private:
  class pqInternals;
  pqInternals *Internals;
  
  PrismView *View;
};

#endif
