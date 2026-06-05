import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import Theme 1.0
import org.qfield 1.0

Item {
  id: topologyCheckerDialog

  property var layerToCheck: null

  visible: false

  Rectangle {
    color: "black"
    opacity: 0.75
    anchors.fill: parent
    MouseArea {
      anchors.fill: parent
      onClicked: topologyCheckerDialog.visible = false
    }
  }

  Rectangle {
    id: dialogBox
    anchors.centerIn: parent
    width: Math.min(parent.width - 40, 500)
    height: Math.min(parent.height - 80, 600)
    radius: 8
    color: Theme.darkGray

    ColumnLayout {
      anchors.fill: parent
      anchors.margins: 16
      spacing: 8

      RowLayout {
        Layout.fillWidth: true

        Label {
          text: qsTr( "Topology Checker" )
          font: Theme.strongFont
          color: "white"
          Layout.fillWidth: true
        }

        Label {
          text: "by Luka Nukradze"
          font: Theme.tipFont
          color: "#4fc3f7"
        }

        QfToolButton {
          iconSource: Theme.getThemeIcon( "ic_close_white_24dp" )
          bgcolor: "transparent"
          width: 32
          height: 32
          onClicked: topologyCheckerDialog.visible = false
        }
      }

      Rectangle {
        Layout.fillWidth: true
        height: 1
        color: "#555555"
      }

      Label {
        id: layerNameLabel
        text: layerToCheck ? qsTr( "Layer: %1" ).arg( layerToCheck.name ) : ""
        font: Theme.tipFont
        color: "#cccccc"
        Layout.fillWidth: true
      }

      RowLayout {
        Layout.fillWidth: true
        spacing: 8

        QfButton {
          text: qsTr( "Run Check" )
          Layout.fillWidth: true
          onClicked: {
            if ( layerToCheck ) {
              topologyCheckerModel.runChecks( layerToCheck )
            }
          }
        }

        QfButton {
          text: qsTr( "Clear" )
          onClicked: {
            topologyCheckerModel.clearResults()
          }
        }
      }

      Label {
        id: statusLabel
        text: topologyCheckerModel.statusText
        font: Theme.tipFont
        color: topologyCheckerModel.hasErrors ? "#ff6b6b" : "#69db7c"
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
        visible: topologyCheckerModel.statusText !== ""
      }

      Rectangle {
        Layout.fillWidth: true
        height: 1
        color: "#555555"
        visible: topologyCheckerModel.count > 0
      }

      ListView {
        id: errorList
        Layout.fillWidth: true
        Layout.fillHeight: true
        model: topologyCheckerModel
        clip: true
        visible: topologyCheckerModel.count > 0

        delegate: Rectangle {
          width: errorList.width
          height: errorRow.height + 12
          color: index % 2 === 0 ? "#2a2a2a" : "#333333"
          radius: 4

          Column {
            id: errorRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: 8
            spacing: 2

            Label {
              width: parent.width
              text: model.errorType
              font.bold: true
              font.pointSize: Theme.tipFont.pointSize
              color: "#ff6b6b"
              wrapMode: Text.WordWrap
            }

            Label {
              width: parent.width
              text: model.featureInfo
              font.pointSize: Theme.tipFont.pointSize - 1
              color: "#cccccc"
              wrapMode: Text.WordWrap
            }
          }

          MouseArea {
            anchors.fill: parent
            onClicked: {
              if ( model.hasGeometry ) {
                mapCanvas.zoomToFeature( model.featureId, layerToCheck )
                topologyCheckerDialog.visible = false
              }
            }
          }
        }
      }

      Label {
        text: qsTr( "No errors found!" )
        font: Theme.strongFont
        color: "#69db7c"
        horizontalAlignment: Text.AlignHCenter
        Layout.alignment: Qt.AlignCenter
        visible: topologyCheckerModel.checked && topologyCheckerModel.count === 0
      }
    }
  }

  TopologyCheckerModel {
    id: topologyCheckerModel
  }
}
