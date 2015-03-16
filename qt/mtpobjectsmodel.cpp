#include "mtpobjectsmodel.h"
#include <QDebug>
#include <QBrush>
#include <QColor>

MtpObjectsModel::MtpObjectsModel(QObject *parent): QAbstractListModel(parent)
{

}

MtpObjectsModel::~MtpObjectsModel()
{

}

void MtpObjectsModel::setParent(mtp::u32 parentObjectId)
{
	beginResetModel();

	_parentObjectId = parentObjectId;
	mtp::msg::ObjectHandles handles = _session->GetObjectHandles(mtp::Session::AllStorages, mtp::Session::AllFormats, parentObjectId);
	_rows.clear();
	_rows.reserve(handles.ObjectHandles.size());
	for(size_t i = 0; i < handles.ObjectHandles.size(); ++i)
	{
		mtp::u32 oid = handles.ObjectHandles[i];
		_rows.append(Row(oid));
	}

	endResetModel();
}

bool MtpObjectsModel::enter(int idx)
{
	if (idx < 0 || idx >= _rows.size())
		return false;

	Row &row = _rows[idx];
	if (row.GetInfo(_session)->ObjectFormat == (mtp::u16)mtp::ObjectFormat::Association)
	{
		setParent(row.ObjectId);
		return true;
	}
	else
		return false;
}

void MtpObjectsModel::setSession(mtp::SessionPtr session)
{
	beginResetModel();
	_session = session;
	setParent(mtp::Session::Root);
	endResetModel();
}

int MtpObjectsModel::rowCount(const QModelIndex &) const
{ return _rows.size(); }

std::shared_ptr<mtp::msg::ObjectInfo> MtpObjectsModel::Row::GetInfo(mtp::SessionPtr session)
{
	if (!_info)
	{
		_info = std::make_shared<mtp::msg::ObjectInfo>();
		try
		{
			*_info = session->GetObjectInfo(ObjectId);
			//qDebug() << QString::fromUtf8(row.Info->Filename.c_str());
		}
		catch(const std::exception &ex)
		{ qDebug() << "failed to get object info " << ex.what(); }
	}
	return _info;
}

bool MtpObjectsModel::removeRows (int row, int count, const QModelIndex & parent )
{
	qDebug() << "remove rows " << row << " " << count;
	beginRemoveRows(parent, row, row + count - 1);
	for(int i = 0; i < count; ++i)
	{
		mtp::u32 oid = objectIdAt(row + i);
		if (oid == 0)
			continue;
		_session->DeleteObject(oid);
	}
	_rows.remove(row, count);

	endRemoveRows();
	return false;
}


mtp::u32 MtpObjectsModel::objectIdAt(int idx)
{
	return (idx >= 0 && idx < _rows.size())? _rows[idx].ObjectId: 0;
}

QVariant MtpObjectsModel::data(const QModelIndex &index, int role) const
{
	int row_idx = index.row();
	if (row_idx < 0 || row_idx > _rows.size())
		return QVariant();

	Row &row = _rows[row_idx];

	switch(role)
	{
	case Qt::DisplayRole:
		return QString::fromUtf8(row.GetInfo(_session)->Filename.c_str());

	case Qt::ForegroundRole:
		return row.GetInfo(_session)->ObjectFormat == (mtp::u16)mtp::ObjectFormat::Association? QBrush(QColor(0, 0, 128)): QBrush(Qt::black);

	default:
		return QVariant();
	}
}

void MtpObjectsModel::createDirectory(const QString &name)
{
	mtp::msg::ObjectInfo oi;
	QByteArray filename = name.toUtf8();
	oi.Filename = filename.data();
	oi.ObjectFormat = (mtp::u16)mtp::ObjectFormat::Association;
	mtp::Session::NewObjectInfo noi = _session->SendObjectInfo(oi, 0, _parentObjectId);
	beginInsertRows(QModelIndex(), _rows.size(), _rows.size());
	_rows.push_back(Row(noi.ObjectId));
	endInsertRows();
}