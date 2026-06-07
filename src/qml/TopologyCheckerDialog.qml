import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import Theme 1.0
import org.qfield 1.0

Item {
  id: root

  property var mapSettingsRef: null
  property bool isOpen: false
  property bool showSettings: false

  visible: isOpen

  // Available vector layer names for the rule editor combos. Refreshed each
  // time the settings panel is opened so it reflects the current project.
  property var layerNames: []

  function refreshLayers() {
    layerNames = topologyModel.vectorLayerNames()
  }

  onShowSettingsChanged: {
    if ( showSettings )
      refreshLayers()
  }

  // ---- Highlight rectangle (drawn exactly on the error after zoom) ----
  // zoomToError() converts the error bbox to screen pixels and emits
  // highlightScreenRequested(x, y, width, height) in this item's coordinates.
  Rectangle {
    id: errorHighlight
    visible: false
    color: "transparent"
    border.color: "#FF6600"
    border.width: 4
    radius: 4
    z: 20
    opacity: 0

    SequentialAnimation {
      id: highlightAnim
      loops: 3
      NumberAnimation { target: errorHighlight; property: "opacity"; to: 1.0; duration: 220 }
      NumberAnimation { target: errorHighlight; property: "opacity"; to: 0.15; duration: 320 }
      onStopped: errorHighlight.visible = false
    }
  }

  Connections {
    target: topologyModel
    onHighlightScreenRequested: {
      errorHighlight.x = x
      errorHighlight.y = y
      errorHighlight.width = Math.max( width, 14 )
      errorHighlight.height = Math.max( height, 14 )
      errorHighlight.opacity = 0
      errorHighlight.visible = true
      highlightAnim.restart()
    }
  }

  // ---- Side panel ----
  Rectangle {
    id: panel
    width: Math.min( parent.width * 0.7, 440 )
    height: parent.height
    anchors.right: parent.right
    color: "white"
    z: 10

    Rectangle {
      width: 4
      height: parent.height
      anchors.left: parent.left
      anchors.leftMargin: -4
      gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0.0; color: "transparent" }
        GradientStop { position: 1.0; color: "#33000000" }
      }
    }

    ColumnLayout {
      anchors.fill: parent
      spacing: 0

      // ---- Header ----
      Rectangle {
        Layout.fillWidth: true
        height: 52
        color: Theme.mainColor

        RowLayout {
          anchors.fill: parent
          anchors.leftMargin: 16
          anchors.rightMargin: 6
          spacing: 4

          Label {
            text: root.showSettings ? qsTr( "წესების პარამეტრები" ) : qsTr( "ტოპოლოგია" )
            font.pixelSize: 18
            font.bold: true
            color: "white"
            Layout.fillWidth: true
            elide: Text.ElideRight
          }

          // Settings / back toggle
          Rectangle {
            width: 36; height: 36; radius: 18
            color: settingsBtnArea.pressed ? Qt.rgba( 0, 0, 0, 0.28 ) : Qt.rgba( 0, 0, 0, 0.15 )
            Layout.alignment: Qt.AlignVCenter
            Label {
              anchors.centerIn: parent
              text: root.showSettings ? "‹" : "⚙"
              font.pixelSize: root.showSettings ? 24 : 18
              font.bold: true; color: "white"
            }
            MouseArea {
              id: settingsBtnArea
              anchors.fill: parent
              onClicked: root.showSettings = !root.showSettings
            }
          }

          // Close
          Rectangle {
            width: 36; height: 36; radius: 18
            color: closeBtnArea.pressed ? Qt.rgba( 0, 0, 0, 0.28 ) : Qt.rgba( 0, 0, 0, 0.15 )
            Layout.alignment: Qt.AlignVCenter
            Label {
              anchors.centerIn: parent
              text: "✕"; font.pixelSize: 16; font.bold: true; color: "white"
            }
            MouseArea {
              id: closeBtnArea
              anchors.fill: parent
              onClicked: root.isOpen = false
            }
          }
        }
      }

      // =================================================================
      //  RESULTS VIEW
      // =================================================================
      Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: !root.showSettings

        ColumnLayout {
          anchors.fill: parent
          spacing: 0

          // ---- Check button ----
          Rectangle {
            Layout.fillWidth: true
            height: 60
            color: "#F5F5F5"

            Rectangle {
              id: checkBtn
              anchors.centerIn: parent
              width: parent.width - 32; height: 42; radius: 6
              color: checkBtnArea.pressed ? Qt.darker( Theme.mainColor, 1.2 ) : Theme.mainColor

              Label {
                anchors.centerIn: parent
                text: qsTr( "შემოწმება" )
                font.pixelSize: 15; font.bold: true; color: "white"
              }

              MouseArea {
                id: checkBtnArea
                anchors.fill: parent
                onClicked: topologyModel.runChecksForCurrentExtent()
              }
            }
          }

          // ---- Status ----
          Label {
            Layout.fillWidth: true
            Layout.leftMargin: 16; Layout.rightMargin: 16
            Layout.topMargin: 6; Layout.bottomMargin: 2
            text: topologyModel.statusText
            font.pixelSize: 13
            color: topologyModel.hasErrors ? "#E53935" : "#43A047"
            wrapMode: Text.WordWrap
            visible: topologyModel.statusText !== ""
          }

          Rectangle {
            Layout.fillWidth: true; height: 1; color: "#E0E0E0"
            visible: topologyModel.count > 0
          }

          // ---- Error list (grouped by sectionText) ----
          ListView {
            id: errorList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: topologyModel
            clip: true
            visible: topologyModel.count > 0

            section.property: "sectionText"
            section.delegate: Rectangle {
              width: errorList.width
              height: 36
              color: "#EEEEEE"

              RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 10
                spacing: 8

                Rectangle {
                  width: 10; height: 10; radius: 5
                  color: "#E53935"
                  Layout.alignment: Qt.AlignVCenter
                }

                Label {
                  text: section
                  font.pixelSize: 13
                  font.bold: true
                  color: "#424242"
                  Layout.fillWidth: true
                  elide: Text.ElideRight
                }
              }

              Rectangle {
                width: parent.width; height: 1
                color: "#BDBDBD"
                anchors.bottom: parent.bottom
              }
            }

            delegate: Rectangle {
              width: errorList.width
              height: 46
              color: delegateArea.pressed ? "#FFF3E0" : "white"

              RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 10
                spacing: 8

                Label {
                  text: qsTr( "ხ. #%1" ).arg( model.displayIndex )
                  font.pixelSize: 14
                  color: "#616161"
                  Layout.fillWidth: true
                }

                Label {
                  text: "›"
                  font.pixelSize: 22
                  color: model.hasGeometry ? "#FF6600" : "#BDBDBD"
                  Layout.alignment: Qt.AlignVCenter
                }
              }

              Rectangle {
                width: parent.width - 28; height: 1
                color: "#F0F0F0"
                anchors.bottom: parent.bottom
                anchors.right: parent.right
              }

              MouseArea {
                id: delegateArea
                anchors.fill: parent
                onClicked: {
                  if ( model.hasGeometry )
                    topologyModel.zoomToError( index )
                }
              }
            }
          }

          // ---- Empty state before check ----
          Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !topologyModel.checked

            Column {
              anchors.centerIn: parent
              spacing: 14

              Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "☑"; font.pixelSize: 52; color: "#BDBDBD"
              }

              Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr( "დააჭირეთ შემოწმება ღილაკს" )
                font.pixelSize: 14; color: "#9E9E9E"
              }
            }
          }

          // ---- No errors state ----
          Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: topologyModel.checked && topologyModel.count === 0

            Column {
              anchors.centerIn: parent
              spacing: 12

              Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "✓"; font.pixelSize: 52; color: "#43A047"
              }

              Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr( "ხარვეზი არ აღმოჩენილა" )
                font.pixelSize: 15; color: "#43A047"
              }
            }
          }
        }
      }

      // =================================================================
      //  SETTINGS VIEW (rule editor, QGIS-style)
      // =================================================================
      Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.showSettings

        ColumnLayout {
          anchors.fill: parent
          spacing: 0

          // ---- Current rules list ----
          ListView {
            id: rulesList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: topologyModel.rules

            header: Rectangle {
              width: rulesList.width
              height: 32
              color: "#EEEEEE"
              Label {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                text: qsTr( "მიმდინარე წესები" )
                font.pixelSize: 13; font.bold: true; color: "#424242"
              }
            }

            delegate: Rectangle {
              width: rulesList.width
              height: 56
              color: "white"

              RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                spacing: 8

                // Enable toggle
                Switch {
                  checked: model.ruleEnabled
                  Layout.alignment: Qt.AlignVCenter
                  onToggled: topologyModel.rules.setRuleEnabled( index, checked )
                }

                ColumnLayout {
                  Layout.fillWidth: true
                  spacing: 1

                  Label {
                    text: model.ruleTypeName
                    font.pixelSize: 13
                    font.bold: true
                    color: model.ruleEnabled ? "#212121" : "#9E9E9E"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                  }
                  Label {
                    text: model.needsLayer2
                          ? ( model.layer1 + "  →  " + model.layer2 )
                          : model.layer1
                    font.pixelSize: 12
                    color: "#757575"
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                  }
                }

                // Delete
                Rectangle {
                  width: 34; height: 34; radius: 17
                  color: delArea.pressed ? "#FFCDD2" : "#FFEBEE"
                  Layout.alignment: Qt.AlignVCenter
                  Label {
                    anchors.centerIn: parent
                    text: "🗑"; font.pixelSize: 15
                  }
                  MouseArea {
                    id: delArea
                    anchors.fill: parent
                    onClicked: topologyModel.rules.removeRule( index )
                  }
                }
              }

              Rectangle {
                width: parent.width; height: 1
                color: "#F0F0F0"
                anchors.bottom: parent.bottom
              }
            }

            // Empty
            Label {
              anchors.centerIn: parent
              visible: topologyModel.rules.count === 0
              text: qsTr( "წესები არ არის.\nდაამატეთ ქვემოთ." )
              horizontalAlignment: Text.AlignHCenter
              font.pixelSize: 13; color: "#9E9E9E"
            }
          }

          Rectangle { Layout.fillWidth: true; height: 1; color: "#E0E0E0" }

          // ---- Add-rule editor ----
          Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: addCol.implicitHeight + 20
            color: "#FAFAFA"

            ColumnLayout {
              id: addCol
              anchors.fill: parent
              anchors.margins: 10
              spacing: 6

              Label {
                text: qsTr( "ახალი წესი" )
                font.pixelSize: 13; font.bold: true; color: "#424242"
              }

              ComboBox {
                id: ruleTypeCombo
                Layout.fillWidth: true
                model: topologyModel.rules.ruleTypeNames()
                currentIndex: 5  // "must not overlap"
              }

              ComboBox {
                id: layer1Combo
                Layout.fillWidth: true
                model: root.layerNames
                displayText: currentIndex < 0 ? qsTr( "ფენა #1" ) : currentText
              }

              ComboBox {
                id: layer2Combo
                Layout.fillWidth: true
                model: root.layerNames
                displayText: currentIndex < 0 ? qsTr( "ფენა #2" ) : currentText
                enabled: topologyModel.rules.ruleTypeNeedsLayer2( ruleTypeCombo.currentIndex )
                opacity: enabled ? 1.0 : 0.4
              }

              RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                  Layout.fillWidth: true
                  height: 40; radius: 6
                  color: addArea.pressed ? Qt.darker( Theme.mainColor, 1.2 ) : Theme.mainColor
                  Label {
                    anchors.centerIn: parent
                    text: qsTr( "დამატება" )
                    font.pixelSize: 14; font.bold: true; color: "white"
                  }
                  MouseArea {
                    id: addArea
                    anchors.fill: parent
                    onClicked: {
                      if ( layer1Combo.currentIndex < 0 )
                        return
                      var needs2 = topologyModel.rules.ruleTypeNeedsLayer2( ruleTypeCombo.currentIndex )
                      if ( needs2 && layer2Combo.currentIndex < 0 )
                        return
                      topologyModel.rules.addRule(
                        ruleTypeCombo.currentIndex,
                        layer1Combo.currentText,
                        needs2 ? layer2Combo.currentText : "" )
                    }
                  }
                }

                Rectangle {
                  Layout.preferredWidth: 100
                  height: 40; radius: 6
                  color: resetArea.pressed ? "#E0E0E0" : "#EEEEEE"
                  border.color: "#BDBDBD"; border.width: 1
                  Label {
                    anchors.centerIn: parent
                    text: qsTr( "ნაგულისხმევი" )
                    font.pixelSize: 12; color: "#424242"
                  }
                  MouseArea {
                    id: resetArea
                    anchors.fill: parent
                    onClicked: topologyModel.rules.resetToDefaults()
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  TopologyCheckerModel {
    id: topologyModel
    mapSettings: root.mapSettingsRef
  }
}
