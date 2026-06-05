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

  // ---- Flash overlay (3-ჯერ ციმციმი) ----
  Rectangle {
    id: flashOverlay
    anchors.fill: parent
    color: "#88FF3300"
    opacity: 0
    z: 99
    visible: opacity > 0

    SequentialAnimation {
      id: flashAnimation
      loops: 3
      NumberAnimation { target: flashOverlay; property: "opacity"; to: 0.7; duration: 180 }
      NumberAnimation { target: flashOverlay; property: "opacity"; to: 0;   duration: 180 }
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

    // Shadow on left edge
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
            width: 36
            height: 36
            radius: 18
            color: Qt.rgba( 0, 0, 0, 0.15 )
            Layout.alignment: Qt.AlignVCenter

            Label {
              anchors.centerIn: parent
              text: "✕"
              font.pixelSize: 16
              font.bold: true
              color: "white"
            }

            MouseArea {
              anchors.fill: parent
              onClicked: root.isOpen = false
            }
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
          width: parent.width - 32
          height: 42
          radius: 6
          color: checkBtnArea.pressed ? Qt.darker( Theme.mainColor, 1.2 ) : Theme.mainColor

          Label {
            anchors.centerIn: parent
            text: qsTr( "შემოწმება" )
            font.pixelSize: 15
            font.bold: true
            color: "white"
          }

          MouseArea {
            id: checkBtnArea
            anchors.fill: parent
            onClicked: {
              if ( root.mapSettingsRef ) {
                var ext = root.mapSettingsRef.extent
                topologyModel.runChecks(
                  ext.xMinimum, ext.yMinimum,
                  ext.xMaximum, ext.yMaximum
                )
              }
            }
          }
        }
      }

      // ---- Status ----
      Label {
        Layout.fillWidth: true
        Layout.leftMargin: 16
        Layout.rightMargin: 16
        Layout.topMargin: 6
        Layout.bottomMargin: 2
        text: topologyModel.statusText
        font.pixelSize: 13
        color: topologyModel.hasErrors ? "#E53935" : "#43A047"
        wrapMode: Text.WordWrap
        visible: topologyModel.statusText !== ""
      }

      // ---- Divider ----
      Rectangle {
        Layout.fillWidth: true
        height: 1
        color: "#E0E0E0"
        visible: topologyModel.count > 0
      }

      // ---- Error list column header ----
      Rectangle {
        Layout.fillWidth: true
        height: 30
        color: "#EEEEEE"
        visible: topologyModel.count > 0

        Label {
          anchors.verticalCenter: parent.verticalCenter
          anchors.left: parent.left
          anchors.leftMargin: 32
          text: qsTr( "ფენა  |  ხარვეზი" )
          font.pixelSize: 11
          font.bold: true
          color: "#757575"
        }
      }

      // ---- Error list ----
      ListView {
        id: errorList
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: topologyModel
        clip: true
        visible: topologyModel.count > 0

        delegate: Rectangle {
          width: errorList.width
          height: errorLabel.implicitHeight + 22
          color: delegateArea.pressed ? "#FFEBEE" : ( index % 2 === 0 ? "white" : "#FAFAFA" )

          RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 14
            anchors.rightMargin: 10
            spacing: 8

            Rectangle {
              width: 7
              height: 7
              radius: 4
              color: "#E53935"
              Layout.alignment: Qt.AlignVCenter
            }

            Label {
              id: errorLabel
              text: model.displayText
              font.pixelSize: 14
              color: "#212121"
              Layout.fillWidth: true
              wrapMode: Text.WordWrap
            }

            Label {
              text: "›"
              font.pixelSize: 22
              color: "#BDBDBD"
              Layout.alignment: Qt.AlignVCenter
            }
          }

          Rectangle {
            width: parent.width
            height: 1
            color: "#EEEEEE"
            anchors.bottom: parent.bottom
          }

          MouseArea {
            id: delegateArea
            anchors.fill: parent
            onClicked: {
              if ( model.hasGeometry ) {
                topologyModel.zoomToError( index )
                flashAnimation.restart()
              }
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
            text: "☑"
            font.pixelSize: 52
            color: "#BDBDBD"
          }

          Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr( "დააჭირეთ „შემოწმება"" )
            font.pixelSize: 14
            color: "#9E9E9E"
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
            text: "✓"
            font.pixelSize: 52
            color: "#43A047"
          }

          Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr( "ხარვეზი არ აღმოჩენილა" )
            font.pixelSize: 15
            color: "#43A047"
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
