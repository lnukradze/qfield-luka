#ifndef TOPOLOGYCHECKERMODEL_H
#define TOPOLOGYCHECKERMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QVector>
#include <qgscoordinatereferencesystem.h>
#include <qgsfeature.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgsspatialindex.h>
#include <qgsvectorlayer.h>

/**
 * One topology check error, ready to be shown in the list and zoomed to.
 * The bbox is stored in MAP CRS so zoom/highlight need no further transform.
 */
struct TopologyError
{
  QString displayText;
  QString sectionText;
  int displayIndex = 0;
  double bboxXMin = 0, bboxYMin = 0, bboxXMax = 0, bboxYMax = 0;
  bool hasGeometry = false;
};

/**
 * A single configurable topology rule, QGIS-style.
 * type is one of TopologyRulesModel::RuleType.
 */
struct TopologyRule
{
  int type = 0;
  QString layer1;
  QString layer2;
  bool enabled = true;
};

/**
 * Model holding the user-configurable list of topology rules (like the QGIS
 * Topology Checker settings). Persisted with QSettings so the configuration
 * survives app restarts. Falls back to a sensible default set on first run.
 */
class TopologyRulesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY( int count READ count NOTIFY countChanged )

  public:
    // Order matches the QGIS "Add Rule" dropdown so combo index == enum value.
    enum RuleType
    {
      MustContain = 0,
      MustNotHaveDuplicates,
      MustNotHaveGaps,
      MustNotHaveInvalidGeometries,
      MustNotHaveMultiPart,
      MustNotOverlap,
      MustNotOverlapWith,
      RuleTypeCount
    };
    Q_ENUM( RuleType )

    enum Roles
    {
      RuleTypeRole = Qt::UserRole + 1,
      RuleTypeNameRole,
      Layer1Role,
      Layer2Role,
      EnabledRole,
      NeedsLayer2Role,
    };
    Q_ENUM( Roles )

    explicit TopologyRulesModel( QObject *parent = nullptr );

    int rowCount( const QModelIndex &parent = QModelIndex() ) const override;
    QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return mRules.count(); }
    const QVector<TopologyRule> &rules() const { return mRules; }

    //! Human readable rule names, in dropdown order (index == RuleType value).
    Q_INVOKABLE QStringList ruleTypeNames() const;
    //! Whether the given rule type uses a second layer.
    Q_INVOKABLE bool ruleTypeNeedsLayer2( int type ) const;

    Q_INVOKABLE void addRule( int type, const QString &layer1, const QString &layer2 );
    Q_INVOKABLE void removeRule( int row );
    Q_INVOKABLE void setRuleEnabled( int row, bool enabled );
    Q_INVOKABLE void resetToDefaults();

    static QString ruleTypeName( int type );

  signals:
    void countChanged();
    void rulesChanged();

  private:
    void load();
    void save() const;
    void loadDefaults();

    QVector<TopologyRule> mRules;
};

/**
 * Runs the configured topology rules against the features visible in the
 * current map extent and exposes the resulting errors as a list model.
 */
class TopologyCheckerModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY( int count READ count NOTIFY countChanged )
    Q_PROPERTY( bool checked READ checked NOTIFY checkedChanged )
    Q_PROPERTY( bool hasErrors READ hasErrors NOTIFY hasErrorsChanged )
    Q_PROPERTY( QString statusText READ statusText NOTIFY statusTextChanged )
    Q_PROPERTY( QObject *mapSettings WRITE setMapSettings )
    Q_PROPERTY( QObject *rules READ rulesObject CONSTANT )

  public:
    enum Roles
    {
      DisplayTextRole = Qt::UserRole + 1,
      HasGeometryRole,
      BboxXMinRole,
      BboxYMinRole,
      BboxXMaxRole,
      BboxYMaxRole,
      SectionTextRole,
      DisplayIndexRole,
    };
    Q_ENUM( Roles )

    explicit TopologyCheckerModel( QObject *parent = nullptr );

    int rowCount( const QModelIndex &parent = QModelIndex() ) const override;
    QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return mErrors.count(); }
    bool checked() const { return mChecked; }
    bool hasErrors() const { return !mErrors.isEmpty(); }
    QString statusText() const { return mStatusText; }

    void setMapSettings( QObject *mapSettings ) { mMapSettings = mapSettings; }
    QObject *rulesObject() const { return mRulesModel; }

    //! Names of all valid vector layers in the current project (for the rule editor combos).
    Q_INVOKABLE QStringList vectorLayerNames() const;

    Q_INVOKABLE void runChecksForCurrentExtent();
    Q_INVOKABLE void clearResults();
    Q_INVOKABLE void zoomToError( int index );

  signals:
    void countChanged();
    void checkedChanged();
    void hasErrorsChanged();
    void statusTextChanged();
    //! Emitted after zoomToError with the error rectangle already in screen pixels.
    void highlightScreenRequested( double x, double y, double width, double height );

  private:
    //! Per-layer cache of features (in layer CRS) built once per run.
    struct LayerCache
    {
      QgsVectorLayer *layer = nullptr;
      QgsSpatialIndex index;
      QHash<QgsFeatureId, QgsGeometry> geoms; // in layer CRS
    };

    QgsVectorLayer *resolveLayer( const QString &name ) const;
    LayerCache *layerCache( QgsVectorLayer *layer, const QgsRectangle &mapExtent );

    QgsRectangle toLayerExtent( const QgsRectangle &mapExtent, QgsVectorLayer *layer ) const;
    QgsRectangle toMapExtent( const QgsRectangle &bboxInLayerCrs, QgsVectorLayer *layer ) const;
    QgsGeometry geomToLayer( const QgsGeometry &geomInOtherCrs, QgsVectorLayer *from, QgsVectorLayer *to ) const;

    void addError( const QString &label, const QgsRectangle &bboxInLayerCrs, QgsVectorLayer *layer );

    void checkInvalidGeometries( LayerCache *lc, const QString &label );
    void checkMultiPart( LayerCache *lc, const QString &label );
    void checkDuplicates( LayerCache *lc, const QString &label );
    void checkOverlapsSelf( LayerCache *lc, const QString &label );
    void checkOverlapsWith( LayerCache *lcA, LayerCache *lcB, const QString &label );
    void checkGaps( LayerCache *lc, const QString &label );
    void checkContains( LayerCache *lcA, LayerCache *lcB, const QString &label );

    void runChecks( const QgsRectangle &mapExtent );

    QVector<TopologyError> mErrors;
    bool mChecked = false;
    QString mStatusText;
    QObject *mMapSettings = nullptr;
    QgsCoordinateReferenceSystem mMapCrs;
    TopologyRulesModel *mRulesModel = nullptr;
    QHash<QString, LayerCache *> mCaches; // keyed by layer id, valid during one run
};

#endif // TOPOLOGYCHECKERMODEL_H
