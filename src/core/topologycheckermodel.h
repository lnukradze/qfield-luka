#ifndef TOPOLOGYCHECKERMODEL_H
#define TOPOLOGYCHECKERMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <qgsvectorlayer.h>

struct TopologyError
{
  QString errorType;
  QString featureInfo;
  QgsFeatureId featureId;
  bool hasGeometry = true;
};

class TopologyCheckerModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY( int count READ count NOTIFY countChanged )
    Q_PROPERTY( bool checked READ checked NOTIFY checkedChanged )
    Q_PROPERTY( bool hasErrors READ hasErrors NOTIFY hasErrorsChanged )
    Q_PROPERTY( QString statusText READ statusText NOTIFY statusTextChanged )

  public:
    enum Roles
    {
      ErrorTypeRole = Qt::UserRole + 1,
      FeatureInfoRole,
      FeatureIdRole,
      HasGeometryRole,
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

    Q_INVOKABLE void runChecks( QgsVectorLayer *layer );
    Q_INVOKABLE void clearResults();

  signals:
    void countChanged();
    void checkedChanged();
    void hasErrorsChanged();
    void statusTextChanged();

  private:
    void checkOverlaps( QgsVectorLayer *layer );
    void checkDuplicates( QgsVectorLayer *layer );
    void checkGaps( QgsVectorLayer *layer );
    void checkInvalidGeometries( QgsVectorLayer *layer );

    QVector<TopologyError> mErrors;
    bool mChecked = false;
    QString mStatusText;
};

#endif // TOPOLOGYCHECKERMODEL_H
