#include "topologycheckermodel.h"

#include <qgsfeatureiterator.h>
#include <qgsgeometry.h>
#include <qgsspatialindex.h>
#include <qgsvectorlayer.h>

TopologyCheckerModel::TopologyCheckerModel( QObject *parent )
  : QAbstractListModel( parent )
{
}

int TopologyCheckerModel::rowCount( const QModelIndex &parent ) const
{
  if ( parent.isValid() )
    return 0;
  return mErrors.count();
}

QVariant TopologyCheckerModel::data( const QModelIndex &index, int role ) const
{
  if ( !index.isValid() || index.row() >= mErrors.count() )
    return QVariant();

  const TopologyError &error = mErrors.at( index.row() );

  switch ( role )
  {
    case ErrorTypeRole:
      return error.errorType;
    case FeatureInfoRole:
      return error.featureInfo;
    case FeatureIdRole:
      return error.featureId;
    case HasGeometryRole:
      return error.hasGeometry;
  }
  return QVariant();
}

QHash<int, QByteArray> TopologyCheckerModel::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles[ErrorTypeRole]   = "errorType";
  roles[FeatureInfoRole] = "featureInfo";
  roles[FeatureIdRole]   = "featureId";
  roles[HasGeometryRole] = "hasGeometry";
  return roles;
}

void TopologyCheckerModel::runChecks( QgsVectorLayer *layer )
{
  if ( !layer || !layer->isValid() )
    return;

  beginResetModel();
  mErrors.clear();
  mChecked = false;
  mStatusText = tr( "Checking…" );
  emit statusTextChanged();
  endResetModel();

  checkInvalidGeometries( layer );
  checkDuplicates( layer );

  if ( layer->geometryType() == QgsWkbTypes::PolygonGeometry )
  {
    checkOverlaps( layer );
    checkGaps( layer );
  }

  mChecked = true;
  int n = mErrors.count();
  mStatusText = n == 0
    ? tr( "No errors found." )
    : tr( "%1 error(s) found." ).arg( n );

  emit countChanged();
  emit checkedChanged();
  emit hasErrorsChanged();
  emit statusTextChanged();
}

void TopologyCheckerModel::clearResults()
{
  beginResetModel();
  mErrors.clear();
  mChecked = false;
  mStatusText = QString();
  endResetModel();

  emit countChanged();
  emit checkedChanged();
  emit hasErrorsChanged();
  emit statusTextChanged();
}

void TopologyCheckerModel::checkInvalidGeometries( QgsVectorLayer *layer )
{
  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature feature;
  while ( it.nextFeature( feature ) )
  {
    if ( !feature.hasGeometry() )
      continue;

    QgsGeometry geom = feature.geometry();
    if ( geom.isNull() || !geom.isGeosValid() )
    {
      QStringList errors;
      geom.validateGeometry( errors );
      TopologyError err;
      err.errorType  = tr( "Invalid geometry" );
      err.featureInfo = tr( "Feature ID: %1 — %2" )
                          .arg( feature.id() )
                          .arg( errors.isEmpty() ? tr( "unknown reason" ) : errors.first() );
      err.featureId   = feature.id();
      err.hasGeometry = true;
      mErrors.append( err );
    }
  }
}

void TopologyCheckerModel::checkDuplicates( QgsVectorLayer *layer )
{
  QgsSpatialIndex index;
  QHash<QgsFeatureId, QgsGeometry> geometries;

  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature feature;
  while ( it.nextFeature( feature ) )
  {
    if ( !feature.hasGeometry() )
      continue;
    index.insertFeature( feature );
    geometries.insert( feature.id(), feature.geometry() );
  }

  QSet<QgsFeatureId> reported;
  for ( auto geomIt = geometries.begin(); geomIt != geometries.end(); ++geomIt )
  {
    QgsFeatureId idA = geomIt.key();
    if ( reported.contains( idA ) )
      continue;

    QList<QgsFeatureId> candidates = index.intersects( geomIt.value().boundingBox() );
    for ( QgsFeatureId idB : candidates )
    {
      if ( idB == idA || reported.contains( idB ) )
        continue;

      if ( geometries[idA].equals( geometries[idB] ) )
      {
        TopologyError err;
        err.errorType   = tr( "Duplicate geometry" );
        err.featureInfo  = tr( "Feature %1 is identical to Feature %2" ).arg( idA ).arg( idB );
        err.featureId    = idA;
        err.hasGeometry  = true;
        mErrors.append( err );
        reported.insert( idA );
        reported.insert( idB );
      }
    }
  }
}

void TopologyCheckerModel::checkOverlaps( QgsVectorLayer *layer )
{
  QgsSpatialIndex index;
  QHash<QgsFeatureId, QgsGeometry> geometries;

  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature feature;
  while ( it.nextFeature( feature ) )
  {
    if ( !feature.hasGeometry() )
      continue;
    index.insertFeature( feature );
    geometries.insert( feature.id(), feature.geometry() );
  }

  QSet<QPair<QgsFeatureId,QgsFeatureId>> reported;
  for ( auto geomIt = geometries.begin(); geomIt != geometries.end(); ++geomIt )
  {
    QgsFeatureId idA = geomIt.key();
    QList<QgsFeatureId> candidates = index.intersects( geomIt.value().boundingBox() );

    for ( QgsFeatureId idB : candidates )
    {
      if ( idB == idA )
        continue;

      auto pair = qMakePair( qMin(idA, idB), qMax(idA, idB) );
      if ( reported.contains( pair ) )
        continue;

      QgsGeometry intersection = geometries[idA].intersection( geometries[idB] );
      if ( !intersection.isNull() && !intersection.isEmpty()
           && intersection.type() == QgsWkbTypes::PolygonGeometry )
      {
        TopologyError err;
        err.errorType   = tr( "Overlap" );
        err.featureInfo  = tr( "Feature %1 overlaps with Feature %2" ).arg( idA ).arg( idB );
        err.featureId    = idA;
        err.hasGeometry  = true;
        mErrors.append( err );
        reported.insert( pair );
      }
    }
  }
}

void TopologyCheckerModel::checkGaps( QgsVectorLayer *layer )
{
  QgsGeometry combined;
  bool first = true;

  QgsFeatureIterator it = layer->getFeatures();
  QgsFeature feature;
  while ( it.nextFeature( feature ) )
  {
    if ( !feature.hasGeometry() )
      continue;
    if ( first )
    {
      combined = feature.geometry();
      first = false;
    }
    else
    {
      combined = combined.combine( feature.geometry() );
    }
  }

  if ( combined.isNull() )
    return;

  QgsGeometry hull = combined.convexHull();
  QgsGeometry gaps = hull.difference( combined );

  if ( !gaps.isNull() && !gaps.isEmpty()
       && gaps.type() == QgsWkbTypes::PolygonGeometry )
  {
    TopologyError err;
    err.errorType   = tr( "Gap" );
    err.featureInfo  = tr( "Gap detected within layer coverage" );
    err.featureId    = -1;
    err.hasGeometry  = false;
    mErrors.append( err );
  }
}
