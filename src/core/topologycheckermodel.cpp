#include "topologycheckermodel.h"

#include <qgscoordinatetransform.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsspatialindex.h>
#include <qgsvectorlayer.h>

// Transform map-canvas extent → layer native CRS
static QgsRectangle toLayerExtent( const QgsRectangle &mapExtent, QgsVectorLayer *layer )
{
  if ( !layer ) return mapExtent;
  QgsCoordinateReferenceSystem mapCrs = QgsProject::instance()->crs();
  if ( !mapCrs.isValid() || !layer->crs().isValid() || layer->crs() == mapCrs )
    return mapExtent;
  QgsCoordinateTransform ct( mapCrs, layer->crs(), QgsProject::instance() );
  return ct.transformBoundingBox( mapExtent );
}

// Transform layer-CRS bbox → map CRS for zoom
static QgsRectangle toMapExtent( const QgsRectangle &bbox, QgsVectorLayer *layer )
{
  if ( !layer ) return bbox;
  QgsCoordinateReferenceSystem mapCrs = QgsProject::instance()->crs();
  if ( !mapCrs.isValid() || !layer->crs().isValid() || layer->crs() == mapCrs )
    return bbox;
  QgsCoordinateTransform ct( layer->crs(), mapCrs, QgsProject::instance() );
  return ct.transformBoundingBox( bbox );
}

static int countFeaturesInExtent( QgsVectorLayer *layer, const QgsRectangle &mapExtent )
{
  if ( !layer ) return -1;
  QgsFeatureRequest req;
  req.setFilterRect( toLayerExtent( mapExtent, layer ) );
  req.setFlags( QgsFeatureRequest::NoGeometry );
  int n = 0;
  QgsFeatureIterator it = layer->getFeatures( req );
  QgsFeature f;
  while ( it.nextFeature( f ) ) ++n;
  return n;
}

TopologyCheckerModel::TopologyCheckerModel( QObject *parent )
  : QAbstractListModel( parent )
{
}

int TopologyCheckerModel::rowCount( const QModelIndex &parent ) const
{
  if ( parent.isValid() ) return 0;
  return mErrors.count();
}

QVariant TopologyCheckerModel::data( const QModelIndex &index, int role ) const
{
  if ( !index.isValid() || index.row() >= mErrors.count() )
    return QVariant();

  const TopologyError &err = mErrors.at( index.row() );
  switch ( role )
  {
    case DisplayTextRole: return err.displayText;
    case HasGeometryRole: return err.hasGeometry;
    case BboxXMinRole:    return err.bboxXMin;
    case BboxYMinRole:    return err.bboxYMin;
    case BboxXMaxRole:    return err.bboxXMax;
    case BboxYMaxRole:    return err.bboxYMax;
  }
  return QVariant();
}

QHash<int, QByteArray> TopologyCheckerModel::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles[DisplayTextRole] = "displayText";
  roles[HasGeometryRole] = "hasGeometry";
  roles[BboxXMinRole]    = "bboxXMin";
  roles[BboxYMinRole]    = "bboxYMin";
  roles[BboxXMaxRole]    = "bboxXMax";
  roles[BboxYMaxRole]    = "bboxYMax";
  return roles;
}

QgsVectorLayer *TopologyCheckerModel::findLayer( const QString &nameHint ) const
{
  const QMap<QString, QgsMapLayer *> layers = QgsProject::instance()->mapLayers();
  for ( QgsMapLayer *ml : layers )
  {
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( ml );
    if ( !vl || !vl->isValid() ) continue;
    if ( vl->name().contains( nameHint, Qt::CaseInsensitive ) )
      return vl;
  }
  return nullptr;
}

void TopologyCheckerModel::addError( const QString &text, const QgsRectangle &bboxInLayerCrs, QgsVectorLayer *layer )
{
  QgsRectangle bbox = toMapExtent( bboxInLayerCrs, layer );
  TopologyError err;
  err.displayText = text;
  err.bboxXMin    = bbox.xMinimum();
  err.bboxYMin    = bbox.yMinimum();
  err.bboxXMax    = bbox.xMaximum();
  err.bboxYMax    = bbox.yMaximum();
  err.hasGeometry = !bbox.isNull();
  mErrors.append( err );
}

void TopologyCheckerModel::runChecks( double xMin, double yMin, double xMax, double yMax )
{
  beginResetModel();
  mErrors.clear();
  mChecked    = false;
  mStatusText = tr( "შემოწმება მიმდინარეობს…" );
  endResetModel();
  emit statusTextChanged();
  emit countChanged();
  emit checkedChanged();
  emit hasErrorsChanged();

  QgsRectangle extent( xMin, yMin, xMax, yMax );

  QgsVectorLayer *nakvetiLayer = findLayer( QStringLiteral( "ნაკვეთ" ) );
  if ( !nakvetiLayer )
    nakvetiLayer = findLayer( QStringLiteral( "საკვეთ" ) );
  QgsVectorLayer *shenobaLayer = findLayer( QStringLiteral( "შენობ" ) );

  // Diagnostic: if nothing found, list all vector layer names
  if ( !nakvetiLayer && !shenobaLayer )
  {
    QStringList names;
    const QMap<QString, QgsMapLayer *> all = QgsProject::instance()->mapLayers();
    for ( QgsMapLayer *ml : all )
    {
      QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( ml );
      if ( vl && vl->isValid() ) names << vl->name();
    }
    mChecked = true;
    mStatusText = names.isEmpty()
      ? tr( "პროექტი გახსნილი არ არის" )
      : tr( "ფენები: " ) + names.join( "; " );
    emit statusTextChanged();
    emit checkedChanged();
    return;
  }

  if ( nakvetiLayer )
  {
    checkInvalidGeometries( nakvetiLayer, QStringLiteral( "ნაკვეთი" ), extent );
    checkDuplicates( nakvetiLayer, QStringLiteral( "ნაკვეთი" ), extent );
    checkOverlapsSelf( nakvetiLayer, QStringLiteral( "ნაკვეთი" ), extent );
    checkGaps( nakvetiLayer, QStringLiteral( "ნაკვეთი" ), extent );
  }

  if ( shenobaLayer )
  {
    checkInvalidGeometries( shenobaLayer, QStringLiteral( "შენობა" ), extent );
    checkDuplicates( shenobaLayer, QStringLiteral( "შენობა" ), extent );
    checkOverlapsSelf( shenobaLayer, QStringLiteral( "შენობა" ), extent );
  }

  if ( nakvetiLayer && shenobaLayer )
  {
    checkOverlapsWith( shenobaLayer, nakvetiLayer,
                       QStringLiteral( "შენობა" ), QStringLiteral( "ნაკვეთი" ), extent );
  }

  mChecked = true;
  int n = mErrors.count();

  QStringList counts;
  if ( nakvetiLayer ) counts << tr( "ნაკვეთი: %1" ).arg( countFeaturesInExtent( nakvetiLayer, extent ) );
  if ( shenobaLayer ) counts << tr( "შენობა: %1" ).arg( countFeaturesInExtent( shenobaLayer, extent ) );
  QString countInfo = counts.join( QStringLiteral( ", " ) );

  mStatusText = n == 0
    ? tr( "%1 — ხარვეზი არ აღმოჩენილა" ).arg( countInfo )
    : tr( "%1 — %2 ხარვეზი აღმოჩენილა" ).arg( countInfo ).arg( n );

  // Reset model so ListView sees the new rows
  beginResetModel();
  endResetModel();
  emit countChanged();
  emit checkedChanged();
  emit hasErrorsChanged();
  emit statusTextChanged();
}

void TopologyCheckerModel::runChecksForCurrentExtent()
{
  QgsRectangle extent;
  if ( mMapSettings )
  {
    QVariant v = mMapSettings->property( "extent" );
    if ( v.canConvert<QgsRectangle>() )
      extent = v.value<QgsRectangle>();
  }

  if ( extent.isEmpty() || extent.isNull() )
  {
    beginResetModel();
    mErrors.clear();
    mChecked    = true;
    mStatusText = tr( "ვერ მოიძებნა რუქის არე (extent)" );
    endResetModel();
    emit statusTextChanged();
    emit countChanged();
    emit checkedChanged();
    emit hasErrorsChanged();
    return;
  }

  runChecks( extent.xMinimum(), extent.yMinimum(), extent.xMaximum(), extent.yMaximum() );
}

void TopologyCheckerModel::clearResults()
{
  beginResetModel();
  mErrors.clear();
  mChecked    = false;
  mStatusText = QString();
  endResetModel();

  emit countChanged();
  emit checkedChanged();
  emit hasErrorsChanged();
  emit statusTextChanged();
}

void TopologyCheckerModel::zoomToError( int index )
{
  if ( !mMapSettings || index < 0 || index >= mErrors.count() )
    return;

  const TopologyError &err = mErrors.at( index );
  QgsRectangle bbox( err.bboxXMin, err.bboxYMin, err.bboxXMax, err.bboxYMax );
  bbox.scale( 2.0 );
  QMetaObject::invokeMethod( mMapSettings, "setExtent",
    Qt::DirectConnection, Q_ARG( QgsRectangle, bbox ) );
}

void TopologyCheckerModel::checkInvalidGeometries( QgsVectorLayer *layer, const QString &name,
                                                    const QgsRectangle &mapExtent )
{
  QgsFeatureRequest req;
  req.setFilterRect( toLayerExtent( mapExtent, layer ) );
  QgsFeatureIterator it = layer->getFeatures( req );
  QgsFeature feature;
  while ( it.nextFeature( feature ) )
  {
    if ( !feature.hasGeometry() ) continue;
    QgsGeometry geom = feature.geometry();
    if ( geom.isNull() || !geom.isGeosValid() )
    {
      addError( name + QStringLiteral( " | არავალიდური გეომეტრია" ),
                geom.boundingBox(), layer );
    }
  }
}

void TopologyCheckerModel::checkDuplicates( QgsVectorLayer *layer, const QString &name,
                                             const QgsRectangle &mapExtent )
{
  QgsSpatialIndex index;
  QHash<QgsFeatureId, QgsGeometry> geoms;
  QgsFeatureRequest req;
  req.setFilterRect( toLayerExtent( mapExtent, layer ) );
  QgsFeatureIterator it = layer->getFeatures( req );
  QgsFeature feature;
  while ( it.nextFeature( feature ) )
  {
    if ( !feature.hasGeometry() ) continue;
    index.addFeature( feature );
    geoms.insert( feature.id(), feature.geometry() );
  }

  QSet<QgsFeatureId> reported;
  for ( auto it2 = geoms.begin(); it2 != geoms.end(); ++it2 )
  {
    QgsFeatureId idA = it2.key();
    if ( reported.contains( idA ) ) continue;
    for ( QgsFeatureId idB : index.intersects( it2.value().boundingBox() ) )
    {
      if ( idB <= idA || reported.contains( idB ) ) continue;
      if ( geoms[idA].equals( geoms[idB] ) )
      {
        addError( name + QStringLiteral( " | დუბლიკატი" ),
                  geoms[idA].boundingBox(), layer );
        reported.insert( idA );
        reported.insert( idB );
      }
    }
  }
}

void TopologyCheckerModel::checkOverlapsSelf( QgsVectorLayer *layer, const QString &name,
                                               const QgsRectangle &mapExtent )
{
  QgsSpatialIndex index;
  QHash<QgsFeatureId, QgsGeometry> geoms;
  QgsFeatureRequest req;
  req.setFilterRect( toLayerExtent( mapExtent, layer ) );
  QgsFeatureIterator it = layer->getFeatures( req );
  QgsFeature feature;
  while ( it.nextFeature( feature ) )
  {
    if ( !feature.hasGeometry() ) continue;
    index.addFeature( feature );
    geoms.insert( feature.id(), feature.geometry() );
  }

  QSet<QPair<QgsFeatureId, QgsFeatureId>> reported;
  for ( auto it2 = geoms.begin(); it2 != geoms.end(); ++it2 )
  {
    QgsFeatureId idA = it2.key();
    for ( QgsFeatureId idB : index.intersects( it2.value().boundingBox() ) )
    {
      if ( idB <= idA ) continue;
      auto pair = qMakePair( idA, idB );
      if ( reported.contains( pair ) ) continue;
      QgsGeometry intersection = geoms[idA].intersection( geoms[idB] );
      // Use area() > 0 — robust against MultiPolygon / GeometryCollection
      if ( !intersection.isNull() && intersection.area() > 1e-10 )
      {
        QString label = name + QStringLiteral( " | გადაფარვა " )
                        + name.toLower() + QStringLiteral( "ებს შორის" );
        addError( label, intersection.boundingBox(), layer );
        reported.insert( pair );
      }
    }
  }
}

void TopologyCheckerModel::checkOverlapsWith( QgsVectorLayer *layerA, QgsVectorLayer *layerB,
                                               const QString &nameA, const QString &nameB,
                                               const QgsRectangle &mapExtent )
{
  // Build spatial index for layerB
  QgsSpatialIndex indexB;
  QHash<QgsFeatureId, QgsGeometry> geomsB;
  QgsFeatureRequest reqB;
  reqB.setFilterRect( toLayerExtent( mapExtent, layerB ) );
  QgsFeatureIterator itB = layerB->getFeatures( reqB );
  QgsFeature fB;
  while ( itB.nextFeature( fB ) )
  {
    if ( !fB.hasGeometry() ) continue;
    indexB.addFeature( fB );
    geomsB.insert( fB.id(), fB.geometry() );
  }

  QgsFeatureRequest reqA;
  reqA.setFilterRect( toLayerExtent( mapExtent, layerA ) );
  QgsFeatureIterator itA = layerA->getFeatures( reqA );
  QgsFeature fA;
  while ( itA.nextFeature( fA ) )
  {
    if ( !fA.hasGeometry() ) continue;
    for ( QgsFeatureId idB : indexB.intersects( fA.geometry().boundingBox() ) )
    {
      QgsGeometry intersection = fA.geometry().intersection( geomsB[idB] );
      if ( !intersection.isNull() && intersection.area() > 1e-10 )
      {
        addError( nameA + QStringLiteral( " | გადაფარვა " ) + nameB + QStringLiteral( "თან" ),
                  intersection.boundingBox(), layerA );
        break;
      }
    }
  }
}

void TopologyCheckerModel::checkGaps( QgsVectorLayer *layer, const QString &name,
                                       const QgsRectangle &mapExtent )
{
  QgsGeometry combined;
  bool first = true;
  QgsFeatureRequest req;
  req.setFilterRect( toLayerExtent( mapExtent, layer ) );
  QgsFeatureIterator it = layer->getFeatures( req );
  QgsFeature feature;
  while ( it.nextFeature( feature ) )
  {
    if ( !feature.hasGeometry() ) continue;
    if ( first ) { combined = feature.geometry(); first = false; }
    else combined = combined.combine( feature.geometry() );
  }
  if ( combined.isNull() ) return;

  QgsGeometry hull = combined.convexHull();
  QgsGeometry gaps = hull.difference( combined );
  if ( !gaps.isNull() && gaps.area() > 1e-10 )
  {
    addError( name + QStringLiteral( " | ხარვეზი" ), gaps.boundingBox(), layer );
  }
}
