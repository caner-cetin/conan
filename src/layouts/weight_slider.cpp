#include "weight_slider.h"
#include <cmath>
#include <cstdint>
#include <qboxlayout.h>
#include <qgridlayout.h>
#include <qlabel.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qslider.h>
#include <qwidget.h>

std::pair<uint8_t, uint8_t> WeightSliderGroup::slider_placement(uint8_t i) {
  auto row = std::floor(i / 2);
  auto height = i % 2;
  return {static_cast<uint8_t>(row), height};
}

WeightSlider::WeightSlider(QString labelText, QWidget *parent) {
  slider = new QSlider(Qt::Horizontal);
  slider->setRange(0, 200);
  slider->setValue(100);
  connect(slider, &QSlider::valueChanged, this, &WeightSlider::update_value);

  value_label = new QLabel();
  value_label->setText("1.0");

  label = new QLabel(parent);
  label->setText(labelText);

  layout = new QHBoxLayout();
  layout->addWidget(label);
  layout->addWidget(slider);
  layout->addWidget(value_label);

  setLayout(layout);

  if (parent) {
    setParent(parent);
  }
}
void WeightSlider::update_value() {
  weight = slider->value() / 100.0;
  value_label->setText(QString::number(weight, 'f', 2));
}

WeightSliderGroup::WeightSliderGroup(QWidget *parent) {
  layout = new QGridLayout();

  rhythm = new WeightSlider("Rhythm");
  tonal = new WeightSlider("Tonal");
  instrumental = new WeightSlider("Instrumental");
  mood = new WeightSlider("Mood");
  genre = new WeightSlider("Genre");
  timbral = new WeightSlider("Timbral");

  sliders = {rhythm, tonal, instrumental, mood, genre, timbral};

  int i = 0;
  for (WeightSlider *slider : sliders) {
    auto rc = slider_placement(i);
    layout->addWidget(slider, rc.first, rc.second);
    i++;
  }

  setLayout(layout);

  if (parent) {
    setParent(parent);
  }
}