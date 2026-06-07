import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import Theme 1.0
import org.qfield 1.0

Item {
  id: root

  property var mapSettingsRef: null
  property bool isOpen: false

  visible: isOpen

  // ---- Highlight rectangle (shown at error location after zoom) ----
  // After zoomToError(), the error bbox fills the center 50% of the map canvas.
  // Panel width = Math.min(parent.width * 0.68, 420) — map canvas = rest on left.
  property real panelW: isOpen ? panel.width : 0
  property real mapW: Math.max( parent.width - panelW, 1 )

  Rectangle {
    id: errorHighlight
    visible: false
    // Center 50% of map canvas area, vertically centered on screen
    x: mapW * 0.25
    y: root.height * 0.25
    width: mapW * 0.5
    height: root.height * 0.5
    color: "transparent"
    border.color: "#FF6600"
    border.width: 4
    radius: 6
    z: 20
    opacity: 0

    SequentialAnimation {
      id: highlightAnim
      loops: 3
      NumberAnimation { target: errorHighlight; property: "opacity"; to: 1.0; duration: 200 }
      NumberAnimation { target: errorHighlight; property: "opacity"; to: 0.15; duration: 300 }
      onStopped: errorHighlight.visible = false
    }
  }

  Connections {
    target: topologyModel
    onHighlightRequested: {
      errorHighlight.opacity = 0
      errorHighlight.visible = true
      highlightAnim.restart()
    }
  }

  // ---- Side panel ----
  Rectangle {
    id: panel
    width: Math.min( parent.width * 0.68, 420 )
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
          anchors.rightMargin: 8

          Label {
            text: qsTr( "ტოპოლოგია" )
            font.pixelSize: 18
            font.bold: true
            color: "white"
            Layout.fillWidth: true
          }

          Rectangle {
            width: 36; height: 36; radius: 18
            color: Qt.rgba( 0, 0, 0, 0.15 )
            Layout.alignment: Qt.AlignVCenter
            Label {
              anchors.centerIn: parent
              text: "✕"; font.pixelSize: 16; font.bold: true; color: "white"
            }
            MouseArea { anchors.fill: parent; onClicked: root.isOpen = false }
          }
        }
      }

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

        // Section grouping — all errors with same sectionText share one header
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

        // Individual error rows
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

  TopologyCheckerModel {
    id: topologyModel
    mapSettings: root.mapSettingsRef
  }
}
