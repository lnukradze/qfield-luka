#include "topologycheckermodel.h"

#include <QPointF>
#include <QSettings>
#include <qgsabstractgeometry.h>
#include <qgscoordinatetransform.h>
#include <qgscurve.h>
#include <qgscurvepolygon.h>
#include <qgsfeatureiterator.h>
#include <qgsfeaturerequest.h>
#include <qgsgeometry.h>
#include <qgspoint.h>
#include <qgspolygon.h>
#include <qgsproject.h>

// Areas (in layer CRS units, m² for the EPSG:32638 cadastral data) below this
// are treated as numeric noise from boundary snapping, not real topology errors.
static const double AREA_TOLERANCE = 1e-6;

// =====================================================================
//  TopologyRulesModel
// =====================================================================

TopologyRulesModel::TopologyRulesModel( QObject *parent )
  : QAbstractListModel( parent )
{
  load();
  if ( mRules.isEmpty() )
    loadDefaults();
}

int TopologyRulesModel::rowCount( const QModelIndex &parent ) const
{
  if ( parent.isValid() ) return 0;
  return mRules.count();
}

QVariant TopologyRulesModel::data( const QModelIndex &index, int role ) const
{
  if ( !index.isValid() || index.row() >= mRules.count() )
    return QVariant();

  const TopologyRule &r = mRules.at( index.row() );
  switch ( role )
  {
    case RuleTypeRole:     return r.type;
    case RuleTypeNameRole: return ruleTypeName( r.type );
    case Layer1Role:       return r.layer1;
    case Layer2Role:       return r.layer2;
    case EnabledRole:      return r.enabled;
    case NeedsLayer2Role:  return ruleTypeNeedsLayer2( r.type );
  }
  return QVariant();
}

QHash<int, QByteArray> TopologyRulesModel::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles[RuleTypeRole]     = "ruleType";
  roles[RuleTypeNameRole] = "ruleTypeName";
  roles[Layer1Role]       = "layer1";
  roles[Layer2Role]       = "layer2";
  roles[EnabledRole]      = "ruleEnabled";
  roles[NeedsLayer2Role]  = "needsLayer2";
  return roles;
}

QString TopologyRulesModel::ruleTypeName( int type )
{
  switch ( type )
  {
    case MustContain:                 return QStringLiteral( "must contain" );
    case MustNotHaveDuplicates:       return QStringLiteral( "must not have duplicates" );
    case MustNotHaveGaps:             return QStringLiteral( "must not have gaps" );
    case MustNotHaveInvalidGeometries:return QStringLiteral( "must not have invalid geometries" );
    case MustNotHaveMultiPart:        return QStringLiteral( "must not have multi-part geometries" );
    case MustNotOverlap:              return QStringLiteral( "must not overlap" );
    case MustNotOverlapWith:          return QStringLiteral( "must not overlap with" );
  }
  return QString();
}

QStringList TopologyRulesModel::ruleTypeNames() const
{
  QStringList names;
  for ( int t = 0; t < RuleTypeCount; ++t )
    names << ruleTypeName( t );
  return names;
}

bool TopologyRulesModel::ruleTypeNeedsLayer2( int type ) const
{
  return type == MustNotOverlapWith || type == MustContain;
}

void TopologyRulesModel::addRule( int type, const QString &layer1, const QString &layer2 )
{
  if ( type < 0 || type >= RuleTypeCount || layer1.isEmpty() )
    return;

  TopologyRule r;
  r.type    = type;
  r.layer1  = layer1;
  r.layer2  = ruleTypeNeedsLayer2( type ) ? layer2 : QString();
  r.enabled = true;

  beginInsertRows( QModelIndex(), mRules.count(), mRules.count() );
  mRules.append( r );
  endInsertRows();
  save();
  emit countChanged();
  emit rulesChanged();
}

void TopologyRulesModel::removeRule( int row )
{
  if ( row < 0 || row >= mRules.count() )
    return;
  beginRemoveRows( QModelIndex(), row, row );
  mRules.removeAt( row );
  endRemoveRows();
  save();
  emit countChanged();
  emit rulesChanged();
}

void TopologyRulesModel::setRuleEnabled( int row, bool enabled )
{
  if ( row < 0 || row >= mRules.count() || mRules[row].enabled == enabled )
    return;
  mRules[row].enabled = enabled;
  const QModelIndex idx = index( row );
  emit dataChanged( idx, idx, { EnabledRole } );
  save();
  emit rulesChanged();
}

void TopologyRulesModel::resetToDefaults()
{
  beginResetModel();
  loadDefaults();
  endResetModel();
  save();
  emit countChanged();
  emit rulesChanged();
}

void TopologyRulesModel::loadDefaults()
{
  mRules.clear();
  const QString nakveti = QStringLiteral( "ნაკვეთი" );
  const QString shenoba = QStringLiteral( "შენობა" );

  mRules.append( { MustNotOverlap,               nakveti, QString(), true } );
  mRules.append( { MustNotOverlapWith,           nakveti, shenoba,   true } );
  mRules.append( { MustNotHaveDuplicates,        nakveti, QString(), true } );
  mRules.append( { MustNotHaveGaps,              nakveti, QString(), true } );
  mRules.append( { MustNotHaveInvalidGeometries, nakveti, QString(), true } );
  mRules.append( { MustNotHaveDuplicates,        shenoba, QString(), true } );
  mRules.append( { MustNotOverlap,               shenoba, QString(), true } );
  mRules.append( { MustNotHaveInvalidGeometries, shenoba, QString(), true } );
}

void TopologyRulesModel::load()
{
  mRules.clear();
  QSettings s;
  // Sentinel so an explicitly-emptied rule list is not overwritten by defaults.
  if ( !s.value( QStringLiteral( "qfield/topology/initialized" ), false ).toBool() )
    return;

  const int n = s.beginReadArray( QStringLiteral( "qfield/topology/rules" ) );
  for ( int i = 0; i < n; ++i )
  {
    s.setArrayIndex( i );
    TopologyRule r;
    r.type    = s.value( QStringLiteral( "type" ) ).toInt();
    r.layer1  = s.value( QStringLiteral( "layer1" ) ).toString();
    r.layer2  = s.value( QStringLiteral( "layer2" ) ).toString();
    r.enabled = s.value( QStringLiteral( "enabled" ), true ).toBool();
    if ( r.type >= 0 && r.type < RuleTypeCount && !r.layer1.isEmpty() )
      mRules.append( r );
  }
  s.endArray();
}

void TopologyRulesModel::save() const
{
  QSettings s;
  s.setValue( QStringLiteral( "qfield/topology/initialized" ), true );
  s.beginWriteArray( QStringLiteral( "qfield/topology/rules" ), mRules.count() );
  for ( int i = 0; i < mRules.count(); ++i )
  {
    s.setArrayIndex( i );
    s.setValue( QStringLiteral( "type" ), mRules[i].type );
    s.setValue( QStringLiteral( "layer1" ), mRules[i].layer1 );
    s.setValue( QStringLiteral( "layer2" ), mRules[i].layer2 );
    s.setValue( QStringLiteral( "enabled" ), mRules[i].enabled );
  }
  s.endArray();
}

// =====================================================================
//  TopologyCheckerModel
// =====================================================================

TopologyCheckerModel::TopologyCheckerModel( QObject *parent )
  : QAbstractListModel( parent )
{
  mRulesModel = new TopologyRulesModel( this );
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
    case DisplayTextRole:  return err.displayText;
    case HasGeometryRole:  return err.hasGeometry;
    case BboxXMinRole:     return err.bboxXMin;
    case BboxYMinRole:     return err.bboxYMin;
    case BboxXMaxRole:     return err.bboxXMax;
    case BboxYMaxRole:     return err.bboxYMax;
    case SectionTextRole:  return err.sectionText;
    case DisplayIndexRole: return err.displayIndex;
  }
  return QVariant();
}

QHash<int, QByteArray> TopologyCheckerModel::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles[DisplayTextRole]  = "displayText";
  roles[HasGeometryRole]  = "hasGeometry";
  roles[BboxXMinRole]     = "bboxXMin";
  roles[BboxYMinRole]     = "bboxYMin";
  roles[BboxXMaxRole]     = "bboxXMax";
  roles[BboxYMaxRole]     = "bboxYMax";
  roles[SectionTextRole]  = "sectionText";
  roles[DisplayIndexRole] = "displayIndex";
  return roles;
}

QStringList TopologyCheckerModel::vectorLayerNames() const
{
  QStringList names;
  const QMap<QString, QgsMapLayer *> layers = QgsProject::instance()->mapLayers();
  for ( QgsMapLayer *ml : layers )
  {
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( ml );
    if ( vl && vl->isValid() )
      names << vl->name();
  }
  names.sort( Qt::CaseInsensitive );
  return names;
}

QgsVectorLayer *TopologyCheckerModel::resolveLayer( const QString &name ) const
{
  if ( name.isEmpty() ) return nullptr;
  const QMap<QString, QgsMapLayer *> layers = QgsProject::instance()->mapLayers();
  // Prefer an exact (case-insensitive) name match.
  for ( QgsMapLayer *ml : layers )
  {
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( ml );
    if ( vl && vl->isValid() && vl->name().compare( name, Qt::CaseInsensitive ) == 0 )
      return vl;
  }
  // Fall back to a substring match (handles "ნაკვეთი (axmeta)" etc.).
  for ( QgsMapLayer *ml : layers )
  {
    QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( ml );
    if ( vl && vl->isValid() && vl->name().contains( name, Qt::CaseInsensitive ) )
      return vl;
  }
  return nullptr;
}

QgsRectangle TopologyCheckerModel::toLayerExtent( const QgsRectangle &mapExtent, QgsVectorLayer *layer ) const
{
  if ( !layer ) return mapExtent;
  if ( !mMapCrs.isValid() || !layer->crs().isValid() || layer->crs() == mMapCrs )
    return mapExtent;
  QgsCoordinateTransform ct( mMapCrs, layer->crs(), QgsProject::instance() );
  return ct.transformBoundingBox( mapExtent );
}

QgsRectangle TopologyCheckerModel::toMapExtent( const QgsRectangle &bboxInLayerCrs, QgsVectorLayer *layer ) const
{
  if ( !layer ) return bboxInLayerCrs;
  if ( !mMapCrs.isValid() || !layer->crs().isValid() || layer->crs() == mMapCrs )
    return bboxInLayerCrs;
  QgsCoordinateTransform ct( layer->crs(), mMapCrs, QgsProject::instance() );
  return ct.transformBoundingBox( bboxInLayerCrs );
}

QgsGeometry TopologyCheckerModel::geomToLayer( const QgsGeometry &geomInOtherCrs,
                                               QgsVectorLayer *from, QgsVectorLayer *to ) const
{
  if ( !from || !to || from->crs() == to->crs() )
    return geomInOtherCrs;
  QgsGeometry g = geomInOtherCrs;
  QgsCoordinateTransform ct( from->crs(), to->crs(), QgsProject::instance() );
  g.transform( ct );
  return g;
}

TopologyCheckerModel::LayerCache *TopologyCheckerModel::layerCache( QgsVectorLayer *layer,
                                                                    const QgsRectangle &mapExtent )
{
  if ( !layer ) return nullptr;
  const QString id = layer->id();
  auto found = mCaches.constFind( id );
  if ( found != mCaches.constEnd() )
    return found.value();

  LayerCache *lc = new LayerCache();
  lc->layer = layer;

  QgsFeatureRequest req;
  req.setFilterRect( toLayerExtent( mapExtent, layer ) );
  QgsFeatureIterator it = layer->getFeatures( req );
  QgsFeature f;
  while ( it.nextFeature( f ) )
  {
    if ( !f.hasGeometry() ) continue;
    QgsGeometry g = f.geometry();
    const bool wasInvalid = g.isNull() || !g.isGeosValid();
    if ( wasInvalid )
    {
      // Invalid WFS geometries break intersection()/unaryUnion(); repair a copy
      // for all geometric ops but remember the feature for the "invalid" rule.
      const QgsGeometry fixed = g.makeValid();
      if ( !fixed.isNull() && !fixed.isEmpty() )
        g = fixed;
      lc->invalidIds.insert( f.id() );
    }
    f.setGeometry( g );
    lc->index.addFeature( f );
    lc->geoms.insert( f.id(), g );
  }

  mCaches.insert( id, lc );
  return lc;
}

void TopologyCheckerModel::addError( const QString &label, const QgsRectangle &bboxInLayerCrs,
                                     QgsVectorLayer *layer )
{
  QgsRectangle bbox = toMapExtent( bboxInLayerCrs, layer );
  TopologyError err;
  err.displayText = label;
  err.sectionText = label;
  err.bboxXMin    = bbox.xMinimum();
  err.bboxYMin    = bbox.yMinimum();
  err.bboxXMax    = bbox.xMaximum();
  err.bboxYMax    = bbox.yMaximum();
  err.hasGeometry = !bbox.isNull();
  mErrors.append( err );
}

// ---------------------------------------------------------------------
//  Individual checks (all operate on cached features in layer CRS)
// ---------------------------------------------------------------------

void TopologyCheckerModel::checkInvalidGeometries( LayerCache *lc, const QString &label )
{
  // invalidIds was populated from the *original* geometry while loading the cache.
  for ( const QgsFeatureId id : qAsConst( lc->invalidIds ) )
  {
    const QgsGeometry geom = lc->geoms.value( id );
    addError( label, geom.boundingBox(), lc->layer );
  }
}

void TopologyCheckerModel::checkMultiPart( LayerCache *lc, const QString &label )
{
  for ( auto it = lc->geoms.constBegin(); it != lc->geoms.constEnd(); ++it )
  {
    const QgsGeometry &geom = it.value();
    if ( !geom.isNull() && geom.isMultipart() && geom.constGet() && geom.constGet()->partCount() > 1 )
      addError( label, geom.boundingBox(), lc->layer );
  }
}

void TopologyCheckerModel::checkDuplicates( LayerCache *lc, const QString &label )
{
  QSet<QgsFeatureId> reported;
  for ( auto it = lc->geoms.constBegin(); it != lc->geoms.constEnd(); ++it )
  {
    const QgsFeatureId idA = it.key();
    if ( reported.contains( idA ) ) continue;
    const QList<QgsFeatureId> cand = lc->index.intersects( it.value().boundingBox() );
    for ( QgsFeatureId idB : cand )
    {
      if ( idB <= idA || reported.contains( idB ) ) continue;
      if ( lc->geoms[idA].equals( lc->geoms[idB] ) )
      {
        addError( label, lc->geoms[idA].boundingBox(), lc->layer );
        reported.insert( idA );
        reported.insert( idB );
        break;
      }
    }
  }
}

void TopologyCheckerModel::checkOverlapsSelf( LayerCache *lc, const QString &label )
{
  QSet<QPair<QgsFeatureId, QgsFeatureId>> reported;
  for ( auto it = lc->geoms.constBegin(); it != lc->geoms.constEnd(); ++it )
  {
    const QgsFeatureId idA = it.key();
    const QList<QgsFeatureId> cand = lc->index.intersects( it.value().boundingBox() );
    for ( QgsFeatureId idB : cand )
    {
      if ( idB <= idA ) continue;
      const auto pair = qMakePair( idA, idB );
      if ( reported.contains( pair ) ) continue;
      const QgsGeometry inter = lc->geoms[idA].intersection( lc->geoms[idB] );
      // Shared edges give a 0-area line; only real 2D overlaps count.
      if ( !inter.isNull() && inter.area() > AREA_TOLERANCE )
      {
        addError( label, inter.boundingBox(), lc->layer );
        reported.insert( pair );
      }
    }
  }
}

void TopologyCheckerModel::checkOverlapsWith( LayerCache *lcA, LayerCache *lcB, const QString &label )
{
  for ( auto it = lcA->geoms.constBegin(); it != lcA->geoms.constEnd(); ++it )
  {
    const QgsGeometry &gA = it.value();
    const QList<QgsFeatureId> cand = lcB->index.intersects( gA.boundingBox() );
    for ( QgsFeatureId idB : cand )
    {
      const QgsGeometry gB = geomToLayer( lcB->geoms[idB], lcB->layer, lcA->layer );
      const QgsGeometry inter = gA.intersection( gB );
      if ( !inter.isNull() && inter.area() > AREA_TOLERANCE )
      {
        addError( label, inter.boundingBox(), lcA->layer );
        break;
      }
    }
  }
}

void TopologyCheckerModel::checkGaps( LayerCache *lc, const QString &label )
{
  // Union every polygon; an enclosed empty area becomes an INTERIOR RING of the
  // union. Those rings are the real gaps. The outer boundary is the exterior
  // ring and is correctly ignored — this is why we do NOT use a convex hull.
  QVector<QgsGeometry> parts;
  parts.reserve( lc->geoms.size() );
  for ( auto it = lc->geoms.constBegin(); it != lc->geoms.constEnd(); ++it )
    parts.append( it.value() );
  if ( parts.isEmpty() ) return;

  const QgsGeometry combined = QgsGeometry::unaryUnion( parts );
  if ( combined.isNull() || combined.isEmpty() ) return;

  const QVector<QgsGeometry> partGeoms = combined.asGeometryCollection();
  for ( const QgsGeometry &part : partGeoms )
  {
    const QgsCurvePolygon *poly = qgsgeometry_cast<const QgsCurvePolygon *>( part.constGet() );
    if ( !poly ) continue;
    for ( int i = 0; i < poly->numInteriorRings(); ++i )
    {
      const QgsCurve *ring = poly->interiorRing( i );
      if ( !ring ) continue;
      std::unique_ptr<QgsPolygon> gapPoly( new QgsPolygon() );
      gapPoly->setExteriorRing( ring->clone() );
      const QgsGeometry gap( gapPoly.release() );
      if ( gap.area() > AREA_TOLERANCE )
        addError( label, gap.boundingBox(), lc->layer );
    }
  }
}

void TopologyCheckerModel::checkContains( LayerCache *lcA, LayerCache *lcB, const QString &label )
{
  // Each layer-1 feature must contain at least one layer-2 feature.
  for ( auto it = lcA->geoms.constBegin(); it != lcA->geoms.constEnd(); ++it )
  {
    const QgsGeometry &gA = it.value();
    bool containsOne = false;
    const QList<QgsFeatureId> cand = lcB->index.intersects( gA.boundingBox() );
    for ( QgsFeatureId idB : cand )
    {
      const QgsGeometry gB = geomToLayer( lcB->geoms[idB], lcB->layer, lcA->layer );
      if ( gA.contains( gB ) )
      {
        containsOne = true;
        break;
      }
    }
    if ( !containsOne )
      addError( label, gA.boundingBox(), lcA->layer );
  }
}

// ---------------------------------------------------------------------
//  Orchestration
// ---------------------------------------------------------------------

void TopologyCheckerModel::runChecks( const QgsRectangle &mapExtent )
{
  beginResetModel();
  mErrors.clear();
  endResetModel();
  emit countChanged();
  emit hasErrorsChanged();

  qDeleteAll( mCaches );
  mCaches.clear();

  const QVector<TopologyRule> rules = mRulesModel->rules();

  QSet<QString> missing;
  int enabledCount = 0;

  for ( const TopologyRule &rule : rules )
  {
    if ( !rule.enabled ) continue;
    ++enabledCount;

    QgsVectorLayer *l1 = resolveLayer( rule.layer1 );
    if ( !l1 )
    {
      missing.insert( rule.layer1 );
      continue;
    }

    const bool needs2 = mRulesModel->ruleTypeNeedsLayer2( rule.type );
    QgsVectorLayer *l2 = needs2 ? resolveLayer( rule.layer2 ) : nullptr;
    if ( needs2 && !l2 )
    {
      missing.insert( rule.layer2 );
      continue;
    }

    LayerCache *lc1 = layerCache( l1, mapExtent );
    LayerCache *lc2 = l2 ? layerCache( l2, mapExtent ) : nullptr;

    const QString name1 = l1->name();
    const QString name2 = l2 ? l2->name() : QString();

    switch ( rule.type )
    {
      case TopologyRulesModel::MustNotOverlap:
        checkOverlapsSelf( lc1, tr( "%1 — გადაფარვა" ).arg( name1 ) );
        break;
      case TopologyRulesModel::MustNotOverlapWith:
        checkOverlapsWith( lc1, lc2, tr( "%1 — გადაფარვა %2-თან" ).arg( name1, name2 ) );
        break;
      case TopologyRulesModel::MustNotHaveDuplicates:
        checkDuplicates( lc1, tr( "%1 — დუბლიკატი" ).arg( name1 ) );
        break;
      case TopologyRulesModel::MustNotHaveGaps:
        checkGaps( lc1, tr( "%1 — ხარვეზი (gap)" ).arg( name1 ) );
        break;
      case TopologyRulesModel::MustNotHaveInvalidGeometries:
        checkInvalidGeometries( lc1, tr( "%1 — არავალიდური გეომეტრია" ).arg( name1 ) );
        break;
      case TopologyRulesModel::MustNotHaveMultiPart:
        checkMultiPart( lc1, tr( "%1 — მრავალნაწილიანი" ).arg( name1 ) );
        break;
      case TopologyRulesModel::MustContain:
        checkContains( lc1, lc2, tr( "%1 — არ შეიცავს %2-ს" ).arg( name1, name2 ) );
        break;
    }
  }

  mChecked = true;
  const int n = mErrors.count();

  // Per-category counts → section header "label (N)" + per-section index.
  {
    QMap<QString, int> catCounts;
    for ( const auto &e : qAsConst( mErrors ) ) catCounts[e.sectionText]++;
    QMap<QString, int> catIdx;
    for ( auto &e : mErrors )
    {
      const QString key = e.sectionText;
      e.sectionText  = key + QStringLiteral( " (%1)" ).arg( catCounts[key] );
      e.displayIndex = ++catIdx[e.sectionText];
    }
  }

  if ( enabledCount == 0 )
  {
    mStatusText = tr( "ჩართული წესი არ არის — გახსენით პარამეტრები" );
  }
  else if ( !missing.isEmpty() )
  {
    const QString found = n == 0
      ? tr( "ხარვეზი არ აღმოჩენილა" )
      : tr( "%1 ხარვეზი" ).arg( n );
    mStatusText = tr( "%1\n⚠ ფენა ვერ მოიძებნა: %2" ).arg( found, QStringList( missing.values() ).join( QStringLiteral( ", " ) ) );
  }
  else
  {
    mStatusText = n == 0
      ? tr( "ხარვეზი არ აღმოჩენილა ✓" )
      : tr( "%1 ხარვეზი აღმოჩენილა" ).arg( n );
  }

  qDeleteAll( mCaches );
  mCaches.clear();

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
  mMapCrs = QgsCoordinateReferenceSystem();

  if ( mMapSettings )
  {
    const QVariant ev = mMapSettings->property( "extent" );
    if ( ev.canConvert<QgsRectangle>() )
      extent = ev.value<QgsRectangle>();

    const QVariant cv = mMapSettings->property( "destinationCrs" );
    if ( cv.canConvert<QgsCoordinateReferenceSystem>() )
      mMapCrs = cv.value<QgsCoordinateReferenceSystem>();
  }

  if ( !mMapCrs.isValid() )
    mMapCrs = QgsProject::instance()->crs();

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

  runChecks( extent );
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
  if ( bbox.isNull() )
    return;

  // Add margin so the error is centred with context; guarantee a usable size
  // for point/zero-area errors by falling back to a fraction of the view.
  double w = bbox.width();
  double h = bbox.height();
  double size = std::max( w, h );
  if ( size <= 0.0 )
  {
    const QVariant ev = mMapSettings->property( "extent" );
    double cur = ev.canConvert<QgsRectangle>() ? ev.value<QgsRectangle>().width() : 0.0;
    size = cur > 0.0 ? cur * 0.02 : 10.0;
  }
  bbox.grow( size * 0.6 );

  // NOTE: setExtent() is the property WRITE accessor, NOT a Q_INVOKABLE/slot, so
  // QMetaObject::invokeMethod( "setExtent" ) silently does nothing. Setting the
  // "extent" property goes through the accessor and actually moves the canvas.
  mMapSettings->setProperty( "extent", QVariant::fromValue( bbox ) );

  // Convert the *error* rectangle (not the padded view) to screen pixels so the
  // highlight sits exactly on the geometry regardless of aspect ratio.
  QgsRectangle errBox( err.bboxXMin, err.bboxYMin, err.bboxXMax, err.bboxYMax );
  if ( errBox.width() <= 0 || errBox.height() <= 0 )
    errBox.grow( size * 0.05 );

  QPointF topLeft, bottomRight;
  bool okTL = QMetaObject::invokeMethod( mMapSettings, "coordinateToScreen",
    Qt::DirectConnection, Q_RETURN_ARG( QPointF, topLeft ),
    Q_ARG( QgsPoint, QgsPoint( errBox.xMinimum(), errBox.yMaximum() ) ) );
  bool okBR = QMetaObject::invokeMethod( mMapSettings, "coordinateToScreen",
    Qt::DirectConnection, Q_RETURN_ARG( QPointF, bottomRight ),
    Q_ARG( QgsPoint, QgsPoint( errBox.xMaximum(), errBox.yMinimum() ) ) );

  if ( okTL && okBR )
  {
    const double x = topLeft.x();
    const double y = topLeft.y();
    const double width = bottomRight.x() - topLeft.x();
    const double height = bottomRight.y() - topLeft.y();
    emit highlightScreenRequested( x, y, width, height );
  }
}
