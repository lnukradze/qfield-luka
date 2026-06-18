#include "shapedigitizingmodel.h"

#include <cmath>
#include <memory>

#include <QVariantMap>
#include <qgsabstractgeometry.h>
#include <qgscircle.h>
#include <qgscoordinatetransform.h>
#include <qgsexception.h>
#include <qgsfeature.h>
#include <qgspoint.h>
#include <qgspolygon.h>
#include <qgsproject.h>
#include <qgsquadrilateral.h>
#include <qgsrectangle.h>
#include <qgsregularpolygon.h>
#include <qgsvectorlayer.h>
#include <qgswkbtypes.h>

ShapeDigitizingModel::ShapeDigitizingModel( QObject *parent )
  : QObject( parent )
{
}

int ShapeDigitizingModel::requiredPoints() const
{
  switch ( mMode )
  {
    case RectangleExtent:   return 2;
    case Rectangle3Points:  return 3;
    case Circle:            return 2;
    case RegularPolygon:    return 2;
  }
  return 2;
}

void ShapeDigitizingModel::setMode( int mode )
{
  if ( mode < 0 || mode >= ShapeModeCount || mode == mMode )
    return;
  mMode = mode;
  mPoints.clear();
  emit modeChanged();
  emit pointsChanged();
}

void ShapeDigitizingModel::setNumSides( int n )
{
  n = std::max( 3, std::min( 100, n ) );
  if ( n == mNumSides )
    return;
  mNumSides = n;
  emit numSidesChanged();
}

QObject *ShapeDigitizingModel::targetLayer() const
{
  return mTargetLayer;
}

void ShapeDigitizingModel::setTargetLayer( QObject *layer )
{
  QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( layer );
  if ( vl == mTargetLayer )
    return;
  mTargetLayer = vl;
  emit targetLayerChanged();
}

QStringList ShapeDigitizingModel::modeNames() const
{
  return QStringList {
    QStringLiteral( "მართკუთხედი (2 კუთხე)" ),
    QStringLiteral( "მართკუთხედი (3 წერტილი)" ),
    QStringLiteral( "წრე (ცენტრი + რადიუსი)" ),
    QStringLiteral( "რეგ. პოლიგონი" )
  };
}

void ShapeDigitizingModel::refreshMapCrs()
{
  mMapCrs = QgsCoordinateReferenceSystem();
  if ( mMapSettings )
  {
    const QVariant cv = mMapSettings->property( "destinationCrs" );
    if ( cv.canConvert<QgsCoordinateReferenceSystem>() )
      mMapCrs = cv.value<QgsCoordinateReferenceSystem>();
  }
  if ( !mMapCrs.isValid() )
    mMapCrs = QgsProject::instance()->crs();
}

QgsPointXY ShapeDigitizingModel::mapCenter() const
{
  if ( mMapSettings )
  {
    const QVariant ev = mMapSettings->property( "extent" );
    if ( ev.canConvert<QgsRectangle>() )
    {
      const QgsRectangle r = ev.value<QgsRectangle>();
      return QgsPointXY( ( r.xMinimum() + r.xMaximum() ) / 2.0,
                         ( r.yMinimum() + r.yMaximum() ) / 2.0 );
    }
  }
  return QgsPointXY();
}

QPointF ShapeDigitizingModel::toScreen( const QgsPointXY &p, bool *ok ) const
{
  QPointF screen;
  bool success = false;
  if ( mMapSettings )
    success = QMetaObject::invokeMethod( mMapSettings, "coordinateToScreen",
      Qt::DirectConnection, Q_RETURN_ARG( QPointF, screen ),
      Q_ARG( QgsPoint, QgsPoint( p.x(), p.y() ) ) );
  if ( ok )
    *ok = success;
  return screen;
}

void ShapeDigitizingModel::capturePoint()
{
  if ( mPoints.count() >= requiredPoints() )
    return; // already have everything this shape needs
  refreshMapCrs();
  mPoints.append( mapCenter() );
  emit pointsChanged();
}

void ShapeDigitizingModel::undoPoint()
{
  if ( mPoints.isEmpty() )
    return;
  mPoints.removeLast();
  emit pointsChanged();
}

void ShapeDigitizingModel::clear()
{
  if ( mPoints.isEmpty() )
    return;
  mPoints.clear();
  emit pointsChanged();
}

QgsGeometry ShapeDigitizingModel::buildGeometryMapCrs( const QVector<QgsPointXY> &pts ) const
{
  if ( pts.count() < requiredPoints() )
    return QgsGeometry();

  std::unique_ptr<QgsPolygon> poly;

  switch ( mMode )
  {
    case RectangleExtent:
    {
      const QgsQuadrilateral q = QgsQuadrilateral::rectangleFromExtent(
        QgsPoint( pts[0].x(), pts[0].y() ),
        QgsPoint( pts[1].x(), pts[1].y() ) );
      if ( q.isValid() )
        poly.reset( q.toPolygon() );
      break;
    }
    case Rectangle3Points:
    {
      const QgsQuadrilateral q = QgsQuadrilateral::rectangleFrom3Points(
        QgsPoint( pts[0].x(), pts[0].y() ),
        QgsPoint( pts[1].x(), pts[1].y() ),
        QgsPoint( pts[2].x(), pts[2].y() ),
        QgsQuadrilateral::Distance );
      if ( q.isValid() )
        poly.reset( q.toPolygon() );
      break;
    }
    case Circle:
    {
      const double r = std::hypot( pts[1].x() - pts[0].x(), pts[1].y() - pts[0].y() );
      if ( r > 0.0 )
      {
        const QgsCircle c( QgsPoint( pts[0].x(), pts[0].y() ), r );
        poly.reset( c.toPolygon( 72 ) );
      }
      break;
    }
    case RegularPolygon:
    {
      const QgsPoint center( pts[0].x(), pts[0].y() );
      const QgsPoint vertex( pts[1].x(), pts[1].y() );
      if ( center != vertex )
      {
        const QgsRegularPolygon rp( center, vertex,
                                    static_cast<unsigned int>( std::max( 3, mNumSides ) ),
                                    QgsRegularPolygon::InscribedCircle );
        poly.reset( rp.toPolygon() );
      }
      break;
    }
  }

  if ( !poly )
    return QgsGeometry();
  return QgsGeometry( poly.release() );
}

QVariantList ShapeDigitizingModel::capturedPointsScreen() const
{
  QVariantList out;
  for ( const QgsPointXY &p : mPoints )
  {
    bool ok = false;
    const QPointF s = toScreen( p, &ok );
    if ( !ok )
      continue;
    QVariantMap m;
    m[QStringLiteral( "x" )] = s.x();
    m[QStringLiteral( "y" )] = s.y();
    out.append( m );
  }
  return out;
}

QVariantList ShapeDigitizingModel::previewOutlineScreen() const
{
  // Captured points + the live crosshair as the provisional next point.
  QVector<QgsPointXY> pts = mPoints;
  if ( pts.count() < requiredPoints() )
    pts.append( mapCenter() );

  const QgsGeometry geom = buildGeometryMapCrs( pts );
  QVariantList out;
  if ( geom.isNull() || geom.isEmpty() )
    return out;

  QgsVertexIterator vit = geom.vertices();
  while ( vit.hasNext() )
  {
    const QgsPoint v = vit.next();
    bool ok = false;
    const QPointF s = toScreen( QgsPointXY( v.x(), v.y() ), &ok );
    if ( !ok )
      continue;
    QVariantMap m;
    m[QStringLiteral( "x" )] = s.x();
    m[QStringLiteral( "y" )] = s.y();
    out.append( m );
  }
  return out;
}

QString ShapeDigitizingModel::commit()
{
  if ( !canCommit() )
    return QStringLiteral( "არასაკმარისი წერტილი" );

  QgsVectorLayer *layer = mTargetLayer;
  if ( !layer || !layer->isValid() )
    return QStringLiteral( "ფენა არ არის არჩეული" );
  if ( layer->geometryType() != QgsWkbTypes::PolygonGeometry )
    return QStringLiteral( "აქტიური ფენა არ არის პოლიგონური" );

  refreshMapCrs();
  QgsGeometry geom = buildGeometryMapCrs( mPoints );
  if ( geom.isNull() || geom.isEmpty() )
    return QStringLiteral( "ფიგურა ვერ აიგო" );

  // Map CRS → layer CRS.
  if ( mMapCrs.isValid() && layer->crs().isValid() && mMapCrs != layer->crs() )
  {
    try
    {
      const QgsCoordinateTransform ct( mMapCrs, layer->crs(), QgsProject::instance() );
      geom.transform( ct );
    }
    catch ( const QgsCsException & )
    {
      return QStringLiteral( "კოორდინატთა გარდაქმნა ჩაიშალა" );
    }
  }

  // Match the layer's single/multi geometry type.
  if ( QgsWkbTypes::isMultiType( layer->wkbType() ) )
    geom.convertToMultiType();

  const bool wasEditing = layer->isEditable();
  if ( !wasEditing && !layer->startEditing() )
    return QStringLiteral( "ფენის რედაქტირება ვერ ჩაირთო" );

  QgsFeature f( layer->fields() );
  f.setGeometry( geom );

  bool ok = layer->addFeature( f );
  if ( ok && !wasEditing )
    ok = layer->commitChanges();
  layer->triggerRepaint();

  if ( !ok )
  {
    emit committed( false, QStringLiteral( "ფიჩერი ვერ დაემატა" ) );
    return QStringLiteral( "ფიჩერი ვერ დაემატა" );
  }

  clear();
  emit committed( true, QStringLiteral( "ფიგურა დაემატა ✓" ) );
  return QString();
}
