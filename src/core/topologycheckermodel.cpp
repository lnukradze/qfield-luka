#include "topologycheckermodel.h"

#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgsproject.h>
#include <qgsspatialindex.h>
#include <qgsvectorlayer.h>

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

void TopologyCheckerModel::addError( const QString &text, const QgsRectangle &bbox )
{
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
  mChecked  = false;
  mStatusText = tr( "შემოწმება მიმდინარეობს…" );
  emit statusTextChanged();
  endResetModel();

  QgsRectangle extent( xMin, yMin, xMax, yMax );

  QgsVectorLayer *nakvetiLayer = findLayer( QStringLiteral( "ნაკვეთ" ) );
  if ( !nakvetiLayer )
    nakvetiLayer = findLayer( QStringLiteral( "საკვეთ" ) );

  QgsVectorLayer *shenobaLayer = findLayer( QStringLiteral( "შენობ" ) );

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
  mStatusText = n == 0
    ? tr( "ხარვეზი არ აღმოჩენილა" )
    : tr( "%1 ხარვეზი აღმოჩენილა" ).arg( n );

  emit countChanged();
  emit checkedChanged();
  emit hasErrorsChanged();
  emit statusTextChanged();
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
                                                    const QgsRectangle &extent )
{
  QgsFeatureRequest req;
  req.setFilterRect( extent );
  QgsFeatureIterator it = layer->getFeatures( req );
  QgsFeature feature;
  while ( it.nextFeature( feature ) )
  {
    if ( !feature.hasGeometry() ) continue;
    QgsGeometry geom = feature.geometry();
    if ( geom.isNull() || !geom.isGeosValid() )
    {
      addError( name + QStringLiteral( " | არავალიდური გეომეტრია" ),
                geom.boundingBox() );
    }
  }
}

void TopologyCheckerModel::checkDuplicates( QgsVectorLayer *layer, const QString &name,
                                             const QgsRectangle &extent )
{
  QgsSpatialIndex index;
  QHash<QgsFeatureId, QgsGeometry> geoms;
  QgsFeatureRequest req;
  req.setFilterRect( extent );
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
                  geoms[idA].boundingBox() );
        reported.insert( idA );
        reported.insert( idB );
      }
    }
  }
}

void TopologyCheckerModel::checkOverlapsSelf( QgsVectorLayer *layer, const QString &name,
                                               const QgsRectangle &extent )
{
  QgsSpatialIndex index;
  QHash<QgsFeatureId, QgsGeometry> geoms;
  QgsFeatureRequest req;
  req.setFilterRect( extent );
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
      if ( !intersection.isNull() && !intersection.isEmpty()
           && intersection.type() == QgsWkbTypes::PolygonGeometry )
      {
        QString label = name + QStringLiteral( " | გადაფარვა " )
                        + name.toLower() + QStringLiteral( "ებს შორის" );
        addError( label, intersection.boundingBox() );
        reported.insert( pair );
      }
    }
  }
}

void TopologyCheckerModel::checkOverlapsWith( QgsVectorLayer *layerA, QgsVectorLayer *layerB,
                                               const QString &nameA, const QString &nameB,
                                               const QgsRectangle &extent )
{
  QgsSpatialIndex indexB;
  QHash<QgsFeatureId, QgsGeometry> geomsB;
  QgsFeatureRequest reqB;
  reqB.setFilterRect( extent );
  QgsFeatureIterator itB = layerB->getFeatures( reqB );
  QgsFeature fB;
  while ( itB.nextFeature( fB ) )
  {
    if ( !fB.hasGeometry() ) continue;
    indexB.addFeature( fB );
    geomsB.insert( fB.id(), fB.geometry() );
  }

  QgsFeatureRequest reqA;
  reqA.setFilterRect( extent );
  QgsFeatureIterator itA = layerA->getFeatures( reqA );
  QgsFeature fA;
  while ( itA.nextFeature( fA ) )
  {
    if ( !fA.hasGeometry() ) continue;
    for ( QgsFeatureId idB : indexB.intersects( fA.geometry().boundingBox() ) )
    {
      QgsGeometry intersection = fA.geometry().intersection( geomsB[idB] );
      if ( !intersection.isNull() && !intersection.isEmpty()
           && intersection.type() == QgsWkbTypes::PolygonGeometry )
      {
        addError( nameA + QStringLiteral( " | გადაფარვა " ) + nameB + QStringLiteral( "თან" ),
                  intersection.boundingBox() );
        break;
      }
    }
  }
}

void TopologyCheckerModel::checkGaps( QgsVectorLayer *layer, const QString &name,
                                       const QgsRectangle &extent )
{
  QgsGeometry combined;
  bool first = true;
  QgsFeatureRequest req;
  req.setFilterRect( extent );
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
  if ( !gaps.isNull() && !gaps.isEmpty()
       && gaps.type() == QgsWkbTypes::PolygonGeometry )
  {
    addError( name + QStringLiteral( " | ხარვეზი" ), gaps.boundingBox() );
  }
}
