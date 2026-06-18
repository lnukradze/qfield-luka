import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import Theme 1.0
import org.qfield 1.0

/**
 * QGIS-style shape digitizing overlay for QField.
 *
 * Two input modes (toggled in the header):
 *  - Pen mode (default): tap the map directly with the pen/finger to place each
 *    point exactly where you touch; the live preview follows the pen. Best for
 *    small/precise objects. Map panning is paused while pen mode is on.
 *  - Centre mode: the map stays pannable; line up the fixed centre crosshair and
 *    tap "წერტილი" to capture the map centre.
 *
 * The captured points build a rectangle / circle / regular polygon, which is
 * added as a new feature to the current editable polygon layer.
 */
Item {
  id: root

  property var mapSettingsRef: null
  property var currentLayer: null
  property bool isOpen: false
  // Pen mode: tap directly on the map (pen/finger) to place each point exactly
  // where you touch. Off = use the fixed centre crosshair + "წერტილი" button.
  property bool penMode: true

  signal toast( string message )

  visible: isOpen

  ShapeDigitizingModel {
    id: shapeModel
    mapSettings: root.mapSettingsRef
    targetLayer: root.currentLayer
  }

  onIsOpenChanged: {
    if ( isOpen ) {
      shapeModel.clear()
      previewCanvas.requestPaint()
    }
  }

  // ---- Live preview of the shape under construction ----
  Canvas {
    id: previewCanvas
    anchors.fill: parent
    visible: root.isOpen
    z: 4

    onPaint: {
      var ctx = getContext( "2d" )
      ctx.reset()

      // Shape outline (captured points + provisional crosshair point)
      var outline = shapeModel.previewOutlineScreen()
      if ( outline.length > 1 ) {
        ctx.beginPath()
        ctx.moveTo( outline[0].x, outline[0].y )
        for ( var i = 1; i < outline.length; i++ )
          ctx.lineTo( outline[i].x, outline[i].y )
        ctx.closePath()
        ctx.fillStyle = "rgba(255, 102, 0, 0.18)"
        ctx.fill()
        ctx.lineWidth = 3
        ctx.strokeStyle = "#FF6600"
        ctx.stroke()
      }

      // Captured construction vertices
      var pts = shapeModel.capturedPointsScreen()
      for ( var j = 0; j < pts.length; j++ ) {
        ctx.beginPath()
        ctx.arc( pts[j].x, pts[j].y, 7, 0, 2 * Math.PI )
        ctx.fillStyle = "#1565C0"
        ctx.fill()
        ctx.lineWidth = 2
        ctx.strokeStyle = "white"
        ctx.stroke()
      }
    }
  }

  Connections {
    target: shapeModel
    onPointsChanged: previewCanvas.requestPaint()
    onModeChanged: previewCanvas.requestPaint()
    onNumSidesChanged: previewCanvas.requestPaint()
  }

  Connections {
    target: root.mapSettingsRef
    ignoreUnknownSignals: true
    onExtentChanged: previewCanvas.requestPaint()
    onVisibleExtentChanged: previewCanvas.requestPaint()
    onRotationChanged: previewCanvas.requestPaint()
  }

  // ---- Pen / finger tap-to-place surface (active in pen mode) ----
  // Tapping places a point exactly where you touch; moving updates the live
  // preview so the shape follows the pen. Panning is paused while pen mode is on.
  MouseArea {
    id: penSurface
    anchors.fill: parent
    enabled: root.isOpen && root.penMode
    visible: enabled
    hoverEnabled: true
    z: 5  // above the preview canvas (4), below the control card (10)
    onPositionChanged: shapeModel.setProvisionalScreenPoint( mouse.x, mouse.y )
    onPressed: shapeModel.setProvisionalScreenPoint( mouse.x, mouse.y )
    onClicked: shapeModel.capturePointAtScreen( mouse.x, mouse.y )
    onExited: shapeModel.clearProvisional()
  }

  // Cursor that follows the pen/finger in pen mode.
  // Turns purple when the point is snapping to existing geometry.
  Rectangle {
    visible: root.isOpen && root.penMode && penSurface.containsMouse
    property color markColor: shapeModel.provisionalSnapped ? "#9b59b6" : "#FF1744"
    x: penSurface.mouseX - width / 2
    y: penSurface.mouseY - height / 2
    width: shapeModel.provisionalSnapped ? 30 : 24
    height: width; radius: width / 2
    color: "transparent"
    border.color: markColor; border.width: shapeModel.provisionalSnapped ? 3 : 2
    z: 7
    Rectangle { anchors.centerIn: parent; width: 5; height: 5; radius: 2.5; color: parent.markColor }
  }

  // ---- Fixed centre crosshair (capture point in centre mode) ----
  Item {
    id: crosshair
    anchors.centerIn: parent
    width: 34; height: 34
    visible: root.isOpen && !root.penMode
    z: 6

    Rectangle { anchors.centerIn: parent; width: 2; height: 34; color: "#FF1744" }
    Rectangle { anchors.centerIn: parent; width: 34; height: 2; color: "#FF1744" }
    Rectangle {
      anchors.centerIn: parent
      width: 12; height: 12; radius: 6
      color: "transparent"
      border.color: "#FF1744"; border.width: 2
    }
  }

  // ---- Control card (bottom, floating) ----
  Rectangle {
    id: panel
    anchors.bottom: parent.bottom
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottomMargin: 20
    width: Math.min( parent.width - 20, 540 )
    height: panelCol.implicitHeight + 20
    radius: 12
    color: "white"
    z: 10

    // Absorb touches so dragging on the card never pans the map.
    MouseArea { anchors.fill: parent; onClicked: {} }

    ColumnLayout {
      id: panelCol
      anchors.fill: parent
      anchors.margins: 10
      spacing: 8

      // Header: title + pen/centre toggle + close
      RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label {
          text: qsTr( "ფიგურის ხაზვა" )
          font.pixelSize: 16; font.bold: true; color: "#212121"
          Layout.fillWidth: true
          elide: Text.ElideRight
        }

        // Pen ⇄ centre toggle
        Rectangle {
          width: penToggleLabel.implicitWidth + 26; height: 38; radius: 19
          color: root.penMode ? Theme.mainColor : "#ECEFF1"
          border.color: root.penMode ? Theme.mainColor : "#B0BEC5"; border.width: 1
          Label {
            id: penToggleLabel
            anchors.centerIn: parent
            text: root.penMode ? qsTr( "✏️ კალამი" ) : qsTr( "🎯 ცენტრი" )
            font.pixelSize: 13; font.bold: true
            color: root.penMode ? "white" : "#37474F"
          }
          MouseArea {
            anchors.fill: parent
            onClicked: { root.penMode = !root.penMode; shapeModel.clearProvisional() }
          }
        }

        // Close (clearly visible)
        Rectangle {
          width: 40; height: 40; radius: 20
          color: closeArea.pressed ? "#B71C1C" : "#E53935"
          Label { anchors.centerIn: parent; text: "✕"; font.pixelSize: 20; font.bold: true; color: "white" }
          MouseArea { id: closeArea; anchors.fill: parent; onClicked: root.isOpen = false }
        }
      }

      // Mode chips
      Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
          model: shapeModel.modeNames()
          Rectangle {
            height: 34; radius: 17
            width: chipLabel.implicitWidth + 24
            color: shapeModel.mode === index ? Theme.mainColor : "#F0F0F0"
            border.color: shapeModel.mode === index ? Theme.mainColor : "#CCCCCC"
            border.width: 1
            Label {
              id: chipLabel
              anchors.centerIn: parent
              text: modelData
              font.pixelSize: 12
              font.bold: shapeModel.mode === index
              color: shapeModel.mode === index ? "white" : "#424242"
            }
            MouseArea { anchors.fill: parent; onClicked: shapeModel.mode = index }
          }
        }
      }

      // Snapping toggle (uses the project's snapping configuration)
      RowLayout {
        Layout.fillWidth: true
        spacing: 8
        Rectangle {
          width: snapLabel.implicitWidth + 28; height: 32; radius: 16
          color: shapeModel.snapEnabled ? "#9b59b6" : "#ECEFF1"
          border.color: shapeModel.snapEnabled ? "#9b59b6" : "#B0BEC5"; border.width: 1
          Label {
            id: snapLabel
            anchors.centerIn: parent
            text: shapeModel.snapEnabled ? qsTr( "🧲 მიკვრა: ჩართ." ) : qsTr( "🧲 მიკვრა: გამორთ." )
            font.pixelSize: 12; font.bold: true
            color: shapeModel.snapEnabled ? "white" : "#37474F"
          }
          MouseArea { anchors.fill: parent; onClicked: shapeModel.snapEnabled = !shapeModel.snapEnabled }
        }
        Item { Layout.fillWidth: true }
      }

      // Sides control (regular polygon only)
      RowLayout {
        Layout.fillWidth: true
        visible: shapeModel.mode === ShapeDigitizingModel.RegularPolygon
        spacing: 8
        Label { text: qsTr( "გვერდები:" ); font.pixelSize: 13; color: "#424242" }
        Rectangle {
          width: 34; height: 34; radius: 6; color: minusArea.pressed ? "#E0E0E0" : "#EEEEEE"
          Label { anchors.centerIn: parent; text: "−"; font.pixelSize: 20; color: "#424242" }
          MouseArea { id: minusArea; anchors.fill: parent; onClicked: shapeModel.numSides = shapeModel.numSides - 1 }
        }
        Label {
          text: shapeModel.numSides
          font.pixelSize: 16; font.bold: true; color: "#212121"
          horizontalAlignment: Text.AlignHCenter
          Layout.preferredWidth: 40
        }
        Rectangle {
          width: 34; height: 34; radius: 6; color: plusArea.pressed ? "#E0E0E0" : "#EEEEEE"
          Label { anchors.centerIn: parent; text: "+"; font.pixelSize: 18; color: "#424242" }
          MouseArea { id: plusArea; anchors.fill: parent; onClicked: shapeModel.numSides = shapeModel.numSides + 1 }
        }
        Item { Layout.fillWidth: true }
      }

      // Status line
      Label {
        Layout.fillWidth: true
        text: qsTr( "წერტილები: %1 / %2" ).arg( shapeModel.pointCount ).arg( shapeModel.requiredPoints )
              + "   ·   " + ( root.currentLayer ? root.currentLayer.name : qsTr( "ფენა არ არის" ) )
        font.pixelSize: 12
        color: "#757575"
        elide: Text.ElideRight
      }

      // Actions
      RowLayout {
        Layout.fillWidth: true
        spacing: 6

        // Capture at centre crosshair (centre mode only)
        Rectangle {
          visible: !root.penMode
          Layout.fillWidth: true
          height: 44; radius: 8
          color: captureArea.pressed ? Qt.darker( Theme.mainColor, 1.2 ) : Theme.mainColor
          Label {
            anchors.centerIn: parent
            text: qsTr( "＋ წერტილი (ცენტრი)" )
            font.pixelSize: 14; font.bold: true; color: "white"
          }
          MouseArea {
            id: captureArea
            anchors.fill: parent
            // Capture at the crosshair (screen centre) so snapping applies here too.
            onClicked: shapeModel.capturePointAtScreen( root.width / 2, root.height / 2 )
          }
        }

        // Pen-mode hint (tap the map instead)
        Rectangle {
          visible: root.penMode
          Layout.fillWidth: true
          height: 44; radius: 8
          color: "#E3F2FD"
          Label {
            anchors.centerIn: parent
            text: qsTr( "✏️ შეახე კალამი რუკას წერტილისთვის" )
            font.pixelSize: 13; color: "#1565C0"
            elide: Text.ElideRight
          }
        }

        // Undo last point
        Rectangle {
          Layout.preferredWidth: 48
          height: 44; radius: 8
          color: undoArea.pressed ? "#E0E0E0" : "#EEEEEE"
          Label { anchors.centerIn: parent; text: "↶"; font.pixelSize: 20; color: "#424242" }
          MouseArea { id: undoArea; anchors.fill: parent; onClicked: shapeModel.undoPoint() }
        }

        // Clear
        Rectangle {
          Layout.preferredWidth: 48
          height: 44; radius: 8
          color: clearArea.pressed ? "#FFCDD2" : "#FFEBEE"
          Label { anchors.centerIn: parent; text: "🗑"; font.pixelSize: 16 }
          MouseArea { id: clearArea; anchors.fill: parent; onClicked: shapeModel.clear() }
        }

        // Commit (save feature)
        Rectangle {
          Layout.preferredWidth: 120
          height: 44; radius: 8
          enabled: shapeModel.canCommit
          opacity: enabled ? 1.0 : 0.4
          color: saveArea.pressed ? "#2E7D32" : "#43A047"
          Label {
            anchors.centerIn: parent
            text: qsTr( "შენახვა" )
            font.pixelSize: 15; font.bold: true; color: "white"
          }
          MouseArea {
            id: saveArea
            anchors.fill: parent
            enabled: shapeModel.canCommit
            onClicked: {
              var err = shapeModel.commit()
              root.toast( err === "" ? qsTr( "ფიგურა დაემატა ✓" ) : err )
            }
          }
        }
      }
    }
  }
}
