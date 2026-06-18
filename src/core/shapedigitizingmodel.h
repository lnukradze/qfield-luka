#ifndef SHAPEDIGITIZINGMODEL_H
#define SHAPEDIGITIZINGMODEL_H

#include <QObject>
#include <QPointF>
#include <QVariantList>
#include <QVector>
#include <qgscoordinatereferencesystem.h>
#include <qgsgeometry.h>
#include <qgspointxy.h>

class QgsVectorLayer;

/**
 * Backs a QGIS-style "shape digitizing" toolbar for QField.
 *
 * The user pans the map so the fixed centre crosshair sits on each construction
 * point, taps "capture", and the model builds the requested shape using the very
 * same helper classes the QGIS desktop shape tools use:
 *   - rectangle from extent / from 3 points  → QgsQuadrilateral
 *   - circle (centre + radius point)          → QgsCircle
 *   - regular polygon (centre + first vertex) → QgsRegularPolygon
 * The finished polygon is added as a new feature to the target editable layer.
 *
 * All captured points are kept in MAP CRS and transformed to the layer CRS on
 * commit, mirroring how TopologyCheckerModel handles the cadastral WFS layers.
 */
class ShapeDigitizingModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY( int mode READ mode WRITE setMode NOTIFY modeChanged )
    Q_PROPERTY( int pointCount READ pointCount NOTIFY pointsChanged )
    Q_PROPERTY( int requiredPoints READ requiredPoints NOTIFY modeChanged )
    Q_PROPERTY( bool canCommit READ canCommit NOTIFY pointsChanged )
    Q_PROPERTY( int numSides READ numSides WRITE setNumSides NOTIFY numSidesChanged )
    Q_PROPERTY( QObject *mapSettings WRITE setMapSettings )
    Q_PROPERTY( QObject *targetLayer READ targetLayer WRITE setTargetLayer NOTIFY targetLayerChanged )

  public:
    // Order matches modeNames() so a combo/button index maps to the enum value.
    enum ShapeMode
    {
      RectangleExtent = 0,  //!< 2 opposite corners (axis-aligned)
      Rectangle3Points,     //!< baseline (2 pts) + perpendicular width (3rd pt)
      Circle,               //!< centre + radius point
      RegularPolygon,       //!< centre + first vertex (N sides)
      ShapeModeCount
    };
    Q_ENUM( ShapeMode )

    explicit ShapeDigitizingModel( QObject *parent = nullptr );

    int mode() const { return mMode; }
    void setMode( int mode );

    int numSides() const { return mNumSides; }
    void setNumSides( int n );

    int pointCount() const { return mPoints.count(); }
    int requiredPoints() const;
    bool canCommit() const { return mPoints.count() >= requiredPoints(); }

    void setMapSettings( QObject *mapSettings ) { mMapSettings = mapSettings; }
    QObject *targetLayer() const;
    void setTargetLayer( QObject *layer );

    //! Human-readable shape names, in enum order (index == ShapeMode value).
    Q_INVOKABLE QStringList modeNames() const;

    //! Capture the current map-centre (crosshair) coordinate as the next point.
    Q_INVOKABLE void capturePoint();
    Q_INVOKABLE void undoPoint();
    Q_INVOKABLE void clear();

    //! Build the shape and add it as a feature to the target layer.
    //! Returns an empty string on success, otherwise a human-readable error.
    Q_INVOKABLE QString commit();

    //! Screen-pixel positions [{x,y}, ...] of the captured construction vertices.
    Q_INVOKABLE QVariantList capturedPointsScreen() const;
    //! Screen-pixel outline [{x,y}, ...] of the shape currently under construction,
    //! using the captured points plus the live crosshair as the provisional next
    //! point, so the preview updates as the user pans the map.
    Q_INVOKABLE QVariantList previewOutlineScreen() const;

  signals:
    void modeChanged();
    void pointsChanged();
    void numSidesChanged();
    void targetLayerChanged();
    void committed( bool ok, const QString &message );

  private:
    void refreshMapCrs();
    QgsPointXY mapCenter() const;
    QgsGeometry buildGeometryMapCrs( const QVector<QgsPointXY> &pts ) const;
    QPointF toScreen( const QgsPointXY &mapPoint, bool *ok ) const;

    int mMode = RectangleExtent;
    int mNumSides = 5;
    QVector<QgsPointXY> mPoints; // in map CRS
    QObject *mMapSettings = nullptr;
    QgsVectorLayer *mTargetLayer = nullptr;
    QgsCoordinateReferenceSystem mMapCrs;
};

#endif // SHAPEDIGITIZINGMODEL_H
