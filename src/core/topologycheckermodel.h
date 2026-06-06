#ifndef TOPOLOGYCHECKERMODEL_H
#define TOPOLOGYCHECKERMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <qgsvectorlayer.h>
#include <qgsgeometry.h>
#include <qgsrectangle.h>
#include <qgscoordinatereferencesystem.h>

struct TopologyError
{
  QString displayText;
  double bboxXMin = 0, bboxYMin = 0, bboxXMax = 0, bboxYMax = 0;
  bool hasGeometry = false;
};

class TopologyCheckerModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY( int count READ count NOTIFY countChanged )
    Q_PROPERTY( bool checked READ checked NOTIFY checkedChanged )
    Q_PROPERTY( bool hasErrors READ hasErrors NOTIFY hasErrorsChanged )
    Q_PROPERTY( QString statusText READ statusText NOTIFY statusTextChanged )
    Q_PROPERTY( QObject *mapSettings WRITE setMapSettings )

  public:
    enum Roles
    {
      DisplayTextRole = Qt::UserRole + 1,
      HasGeometryRole,
      BboxXMinRole,
      BboxYMinRole,
      BboxXMaxRole,
      BboxYMaxRole,
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

    Q_INVOKABLE void runChecks( double xMin, double yMin, double xMax, double yMax );
    Q_INVOKABLE void runChecksForCurrentExtent();
    Q_INVOKABLE void clearResults();
    Q_INVOKABLE void zoomToError( int index );

  signals:
    void countChanged();
    void checkedChanged();
    void hasErrorsChanged();
    void statusTextChanged();

  private:
    QgsVectorLayer *findLayer( const QString &nameHint ) const;
    QgsRectangle toLayerExtent( const QgsRectangle &mapExtent, QgsVectorLayer *layer ) const;
    QgsRectangle toMapExtent( const QgsRectangle &bboxInLayerCrs, QgsVectorLayer *layer ) const;
    int countFeaturesInExtent( QgsVectorLayer *layer, const QgsRectangle &mapExtent ) const;
    void addError( const QString &text, const QgsRectangle &bboxInLayerCrs, QgsVectorLayer *layer );
    void checkOverlapsSelf( QgsVectorLayer *layer, const QString &name, const QgsRectangle &mapExtent );
    void checkOverlapsWith( QgsVectorLayer *layerA, QgsVectorLayer *layerB,
                            const QString &nameA, const QString &nameB, const QgsRectangle &mapExtent );
    void checkDuplicates( QgsVectorLayer *layer, const QString &name, const QgsRectangle &mapExtent );
    void checkGaps( QgsVectorLayer *layer, const QString &name, const QgsRectangle &mapExtent );
    void checkInvalidGeometries( QgsVectorLayer *layer, const QString &name, const QgsRectangle &mapExtent );

    QVector<TopologyError> mErrors;
    bool mChecked = false;
    QString mStatusText;
    QObject *mMapSettings = nullptr;
    QgsCoordinateReferenceSystem mMapCrs;
};

#endif // TOPOLOGYCHECKERMODEL_H
