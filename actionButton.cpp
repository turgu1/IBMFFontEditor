#include "actionButton.h"

ActionButton::ActionButton(QWidget *parent) : QPushButton(parent) {}

void ActionButton::setAction(QAction *action) {

  // if I've got already an action associated to the button
  // remove all connections
  if (_actionOwner && _actionOwner != action) {
    disconnect(_actionOwner, &QAction::changed, this, &ActionButton::updateButtonStatusFromAction);
    disconnect(this, &ActionButton::clicked, _actionOwner, &QAction::trigger);
  }

  // store the action
  _actionOwner = action;

  // configure the button
  updateButtonStatusFromAction();

  // connect the action and the button
  // so that when the action is changed the
  // button is changed too!
  connect(action, &QAction::changed, this, &ActionButton::updateButtonStatusFromAction);

  // connect the button to the slot that forwards the
  // signal to the action
  connect(this, &ActionButton::clicked, _actionOwner, &QAction::trigger);
}

void ActionButton::updateButtonStatusFromAction() {

  if (!_actionOwner) return;
  // setText(_actionOwner->text());
  setStatusTip(_actionOwner->statusTip());
  setToolTip(_actionOwner->toolTip());
  setIcon(_actionOwner->icon());
  setEnabled(_actionOwner->isEnabled());
  setCheckable(_actionOwner->isCheckable());
  setChecked(_actionOwner->isChecked());
}
