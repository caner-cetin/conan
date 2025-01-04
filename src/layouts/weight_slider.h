

#include <QGroupBox>
#include <QSlider>
#include <qboxlayout.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qwidget.h>
class WeightSlider : public QWidget {
  Q_OBJECT
public:
  WeightSlider(QString label, QWidget *parent = nullptr);
  float weight;

private:
  QLabel *label;
  QSlider *slider;
  QLabel *value_label;
  QHBoxLayout *layout;

private Q_SLOTS:
  void update_value();
};
class WeightSliderGroup : public QGroupBox {
  Q_OBJECT
public:
  WeightSliderGroup(QWidget *parent = nullptr);

private:
  WeightSlider *rhythm;
  WeightSlider *tonal;
  WeightSlider *instrumental;
  WeightSlider *mood;
  WeightSlider *genre;
  WeightSlider *timbral;

  std::array<WeightSlider *, 6> sliders;
  std::pair<uint8_t, uint8_t> slider_placement(uint8_t i);

  QGridLayout *layout;
};
