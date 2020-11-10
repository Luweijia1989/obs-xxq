#include "gift-tv.h"
#include "renderer.h"
#include <QJsonDocument>
#include <QDebug>
int gridWidth = 456;
int gridHeight = 120;
int dis[2] = {7 * 24 * 3600 * 1000, 24 * 3600 * 1000};
#define GiftPlaceholder QString("Placeholder")

GiftTV::GiftTV(QObject *parent /* = nullptr */) : QmlSourceBase(parent)
{
	m_timer = new QTimer;
	m_timer->setInterval(1000);
	m_grid = QVector<QVector<QJsonObject>>(5, QVector<QJsonObject>(2));
	addProperties("gifttvProperties", this);

	connect(m_timer, &QTimer::timeout, this, [=]() {
		QJsonArray array = getGiftListByMap();
		if (array.size()) {
			for (int i = 0; i < array.size(); i++) {
				auto obj = array.at(i).toObject();
				long long time = QDateTime::currentDateTime()
							 .toMSecsSinceEpoch();
				if (time - obj["timeStamp"]
						    .toVariant()
						    .toLongLong() >=
				    modeList[m_mode - 1]) {
					QJsonObject giftInfo =
						m_grid[obj["x"].toInt()]
						      [obj["y"].toInt()];

					m_grid[obj["x"].toInt()]
					      [obj["y"].toInt()] =
						      QJsonObject();
					if (giftInfo["giftType"] == 1) {
						m_grid[obj["x"].toInt()]
						      [obj["y"].toInt() + 1] =
							      QJsonObject();
					}

					notifyQMLDeleteGift(obj);
				}
			}
		}

		updateMap();
	});
}

GiftTV::~GiftTV()
{
	m_timer->stop();
	m_timer->deleteLater();
}

void GiftTV::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/GiftTV.qml");
}

void GiftTV::setTriggerCondition(int triggerCondition)
{
	m_triggerCondition = triggerCondition;
}

void GiftTV::setTriggerConditionValue(int triggerConditionValue)
{
	m_triggerConditionValue - triggerConditionValue;
}

void GiftTV::refurshGrid(int rows, int cols)
{
	if (m_grid.size() == rows && m_grid.at(0).size() == cols)
		return;

	int oldRows = m_grid.size();
	int oldCols = m_grid.at(0).size();
	QMap<QString, QJsonObject> tempMap;

	bool hanAdvancedGift = false;
	for (int i = 0; i < oldRows; ++i) {
		for (int j = 0; j < oldCols; ++j) {
			QJsonObject giftInfo = m_grid[i][j];
			if (!giftInfo.isEmpty() &&
			    !giftInfo["isPlaceholder"].toBool()) {
				long long time = QDateTime::currentDateTime()
							 .toMSecsSinceEpoch() -
						 giftInfo["timeStamp"]
							 .toVariant()
							 .toLongLong();
				if (m_mode == 0 && m_disappear < 2 &&
				    time >= dis[m_disappear])
					continue;

				tempMap.insert(
					giftInfo["hitBatchId"].toString(),
					giftInfo);

				if (giftInfo["giftType"].toInt() == 1)
					hanAdvancedGift = true;
			}
		}
	}

	if (hanAdvancedGift && cols == 1) {
		obs_source_t *source = this->source();
		obs_data_t *settings = obs_source_get_settings(source);
		obs_data_release(settings);
		obs_data_set_int(settings, "width",
				 oldCols * 456 + (oldCols - 1) * 20);
		obs_data_set_int(settings, "height", oldRows * 120);
		baseUpdate(settings);

		autoExtendTvCols(oldCols);
		return;
	}

	m_grid =
		QVector<QVector<QJsonObject>>(rows, QVector<QJsonObject>(cols));

	QMap<QString, QJsonObject>::iterator iter = tempMap.begin();
	while (iter != tempMap.end()) {
		QJsonObject giftInfo = iter.value();
		if (m_triggerCondition == 1 &&
		    giftInfo["giftPrice"].toInt() < m_triggerConditionValue) {
			continue;
		}

		getPositonForNewGiftToGrid(giftInfo);
		iter++;
	}

	updateMap();

	notifyQMLLoadArray();
}

void GiftTV::loadGiftArray(QStringList array)
{
	if (m_load)
		return;

	m_load = true;
	if (array.isEmpty())
		return;

	for (auto item : array) {
		QJsonParseError error;
		QJsonDocument document =
			QJsonDocument::fromJson(item.toUtf8(), &error);
		if (error.error == QJsonParseError::NoError) {
			QJsonObject giftInfo = document.object();
			if (m_triggerCondition == 1 &&
			    giftInfo["giftPrice"].toInt() <
				    m_triggerConditionValue) {
				continue;
			}

			long long time =
				QDateTime::currentDateTime()
					.toMSecsSinceEpoch() -
				giftInfo["timeStamp"].toVariant().toLongLong();
			if (m_mode == 0 && m_disappear < 2 &&
			    time >= dis[m_disappear])
				continue;

			m_giftInfoMap.insert(giftInfo["hitBatchId"].toString(),
					     giftInfo);
		}
	}

	QMap<QString, QJsonObject>::iterator iter = m_giftInfoMap.begin();
	while (iter != m_giftInfoMap.end()) {
		QJsonObject giftInfo = iter.value();
		int row = giftInfo["x"].toInt();
		int col = giftInfo["y"].toInt();

		m_grid[row][col] = giftInfo;
		if (giftInfo["giftType"].toInt() == 1) {
			giftInfo["isPlaceholder"] = true;
			m_grid[row][col + 1] = giftInfo;
		}

		iter++;
	}

	updateMap();

	notifyQMLLoadArray();
}

bool GiftTV::findAdvancedGrid(int rows, int cols, QJsonObject &giftInfo)
{
	int tempRows = 0;
	int tempCols = 0;
	int row = 0;
	int col = 0;

	if (m_preferLayout == Horizontal) {
		tempRows = rows;
		tempCols = cols;
	} else {
		tempRows = cols;
		tempCols = rows;
	}

	for (int i = 0; i < tempRows; ++i) {
		for (int j = 0; j < tempCols; ++j) {
			if (m_preferLayout == Horizontal) {
				row = i;
				col = j;
			} else {
				row = j;
				col = i;
			}

			qDebug() << m_preferLayout << tempCols << tempRows;

			if (col < cols - 1 && m_grid[row][col].isEmpty() &&
			    m_grid[row][col + 1].isEmpty()) {
				giftInfo["x"] = row;
				giftInfo["y"] = col;
				giftInfo["xPosition"] = col * (gridWidth + 20);
				giftInfo["yPosition"] = row * gridHeight;
				giftInfo["isNew"] = false;
				m_grid[row][col] = giftInfo;
				QJsonObject temp = giftInfo;
				temp["isPlaceholder"] = true;
				m_grid[row][col + 1] = temp;
				return true;
			}
		}
	}

	return false;
}

bool GiftTV::findNormalGrid(int rows, int cols, QJsonObject &giftInfo)
{
	int tempRows = 0;
	int tempCols = 0;
	int row = 0;
	int col = 0;

	if (m_preferLayout == Horizontal) {
		tempRows = rows;
		tempCols = cols;
	} else {
		tempRows = cols;
		tempCols = rows;
	}

	for (int i = 0; i < tempRows; ++i) {
		for (int j = 0; j < tempCols; ++j) {
			if (m_preferLayout == Horizontal) {
				row = i;
				col = j;
			} else {
				row = j;
				col = i;
			}

			if (m_grid[row][col].isEmpty()) {
				giftInfo["x"] = row;
				giftInfo["y"] = col;
				giftInfo["xPosition"] = col * (gridWidth + 20);
				giftInfo["yPosition"] = row * gridHeight + 10;
				giftInfo["isNew"] = false;
				m_grid[row][col] = giftInfo;
				return true;
			}
		}
	}

	return false;
}

void GiftTV::removeGrid()
{
	int rows = m_grid.size();
	int cols = m_grid.at(0).size();
	QJsonObject minGift;
	int tempRow = -1;
	int tempCol = -1;
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			QJsonObject giftInfo = m_grid[i][j];
			if (giftInfo.isEmpty()) {
				continue;
			}

			if (!giftInfo.isEmpty() &&
			    giftInfo["isPlaceholder"].toBool()) {
				continue;
			}

			QString hitBatchId = giftInfo["hitBatchId"].toString();
			if (!m_giftInfoMap.contains(hitBatchId)) {
				qDebug() << "removeGrid:" << hitBatchId
					 << "is not exist. row:" << i
					 << "col:" << j;
				m_grid[i][j] = QJsonObject();
				continue;
			}

			//初始化礼物表中的最小总价值
			if (minGift.isEmpty() &&
			    m_giftInfoMap.contains(hitBatchId)) {
				minGift = giftInfo;
				tempRow = i;
				tempCol = j;
			} else {
				int tempDiamond = giftInfo["diamond"].toInt();
				long long tempTimeStamp = giftInfo["timeStamp"]
								  .toVariant()
								  .toLongLong();
				int min = minGift["diamond"].toInt();
				if (tempDiamond <= min) {
					if (tempDiamond < min) {
						minGift = giftInfo;
						tempRow = i;
						tempCol = j;
					} else {
						if (tempTimeStamp <
						    minGift["timeStamp"]
							    .toVariant()
							    .toLongLong()) {
							minGift = giftInfo;
							tempRow = i;
							tempCol = j;
						}
					}
				}
			}
		}
	}

	if (minGift["giftType"].toInt() == 0) {
		m_grid[tempRow][tempCol] = QJsonObject();
	} else {
		m_grid[tempRow][tempCol] = QJsonObject();
		m_grid[tempRow][tempCol + 1] = QJsonObject();
	}

	//释放查找出的总价值最低的礼物对应的栅格位置
	notifyQMLDeleteGift(minGift);

	qDebug() << "free grid, row:" << tempRow << "col:" << tempCol
		 << "min gift type" << minGift["giftType"].toInt();
	gridPrintf();
}

void GiftTV::getPositonForNewGiftToGrid(QJsonObject &giftInfo)
{
	int rows = m_grid.size();
	int cols = m_grid.at(0).size();
	int tempRows = rows;
	int tempCols = cols;

	GiftPositon positon;
	int type = giftInfo["giftType"].toInt();
	bool find = false;
	if (type == 0)
		find = findNormalGrid(tempRows, tempCols, giftInfo);
	else
		find = findAdvancedGrid(tempRows, tempCols, giftInfo);

	while (!find) {
		removeGrid();
		if (type == 0)
			find = findNormalGrid(tempRows, tempCols, giftInfo);
		else
			find = findAdvancedGrid(tempRows, tempCols, giftInfo);
	}
}

void GiftTV::updatePrefer(int prefer)
{
	m_preferLayout = (PreferLayout)prefer;
}

void GiftTV::updateMode(int mode)
{
	m_mode = mode;
	QTimer::singleShot(0, this, [=]() {
		if (m_mode >= 1) {
			if (!m_timer->isActive()) {
				m_timer->start();
			}
		} else
			m_timer->stop();
	});
}

void GiftTV::updateDisappear(int disappear)
{
	m_disappear = disappear;
}

void GiftTV::add(QString data)
{
	if (data.isEmpty())
		return;

	QJsonParseError error;
	QJsonDocument document = QJsonDocument::fromJson(data.toUtf8(), &error);
	if (error.error == QJsonParseError::NoError) {
		QJsonObject obj = document.object();

		if (m_triggerCondition == 1 &&
		    obj["giftPrice"].toInt() < m_triggerConditionValue) {
			return;
		}

		QString hitBatchId = obj["hitBatchId"].toString();
		if (m_giftInfoMap.contains(hitBatchId)) {
			int type =
				m_giftInfoMap[hitBatchId]["giftType"].toInt();

			if (m_giftInfoMap[hitBatchId]["hitCount"]
				    .toVariant()
				    .toInt() >
			    obj["hitCount"].toVariant().toInt()) {
				return;
			}

			int x = m_giftInfoMap[hitBatchId]["x"].toInt();
			int y = m_giftInfoMap[hitBatchId]["y"].toInt();
			m_giftInfoMap[hitBatchId]["count"] =
				obj["count"].toString();
			m_giftInfoMap[hitBatchId]["hitCount"] =
				obj["hitCount"].toString();
			m_giftInfoMap[hitBatchId]["rangeLevel"] =
				obj["rangeLevel"].toInt();
			m_giftInfoMap[hitBatchId]["barPercent"] =
				obj["barPercent"].toInt();
			m_grid[x][y] = m_giftInfoMap[hitBatchId];

			if (type == 1) {
				QJsonObject info = m_giftInfoMap[hitBatchId];
				info["isPlaceholder"] = true;
				m_grid[x][y + 1] = info;
			}
			notifyQMLUpdateGift(m_grid[x][y]);
		} else {
			getPositonForNewGiftToGrid(obj);

			notifyQMLInsertNewGift(obj);

			updateMap();
		}
	}
}

void GiftTV::stopTimer()
{
	m_timer->stop();
}

void GiftTV::gridPrintf()
{
	qDebug() << "Grid" << m_grid.size() << m_grid.at(0).size();
	int rows = m_grid.size();
	int cols = m_grid.at(0).size();
	for (int i = 0; i < rows; i++) {
		QStringList list;
		for (int j = 0; j < cols; j++) {
			list.append(m_grid[i][j].isEmpty() ? "0" : "1");
		}
		qDebug() << list;
	}
}

int GiftTV::mode()
{
	return m_mode;
}

int GiftTV::disappear()
{
	return m_disappear;
}

bool GiftTV::needExtendCols(int cols)
{
	if (cols > 1)
		return false;

	int oldRows = m_grid.size();
	int oldCols = m_grid.at(0).size();

	for (int i = 0; i < oldRows; ++i) {
		for (int j = 0; j < oldCols; ++j) {
			QJsonObject giftInfo = m_grid[i][j];
			if (!giftInfo.isEmpty() &&
			    giftInfo["giftType"].toInt() == 1) {
				autoExtendTvCols(oldCols);
				return true;
			}
		}
	}
}

int GiftTV::currentRows()
{
	return m_grid.size();
}

int GiftTV::currentCols()
{
	return m_grid.at(0).size();
}

void GiftTV::notifyQMLLoadArray()
{
	QJsonArray giftList = getGiftListByMap();
	setgiftArray(QJsonDocument(giftList).toJson(QJsonDocument::Compact));
}

void GiftTV::notifyQMLInsertNewGift(QJsonObject &giftInfo)
{
	obs_source_t *source = this->source();
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_release(settings);
	obs_data_erase(settings, "gift");

	setgift(QJsonDocument(giftInfo).toJson(QJsonDocument::Compact));
}

void GiftTV::notifyQMLUpdateGift(QJsonObject &giftInfo)
{
	obs_source_t *source = this->source();
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_release(settings);
	obs_data_erase(settings, "gift");
	setupdateGift(QJsonDocument(giftInfo).toJson(QJsonDocument::Compact));
}

void GiftTV::notifyQMLDeleteGift(QJsonObject &giftInfo)
{
	setinvalidGift(QJsonDocument(giftInfo).toJson(QJsonDocument::Compact));
}

void GiftTV::updateMap()
{
	int rows = m_grid.size();
	int cols = m_grid.at(0).size();
	QMap<QString, QJsonObject> tempMap;

	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			QJsonObject giftInfo = m_grid[i][j];
			if (!giftInfo.isEmpty() &&
			    !giftInfo["isPlaceholder"].toBool()) {
				tempMap.insert(
					giftInfo["hitBatchId"].toString(),
					giftInfo);
			}
		}
	}

	m_giftInfoMap = tempMap;
	gridPrintf();
}

QJsonArray GiftTV::getGiftListByMap()
{
	QJsonArray array;
	QMap<QString, QJsonObject>::iterator iter = m_giftInfoMap.begin();
	while (iter != m_giftInfoMap.end()) {
		array.append(iter.value());
		iter++;
	}
	return array;
}

void GiftTV::autoExtendTvCols(int cols)
{
	obs_source_t *source = this->source();
	obs_data_t *event = obs_data_create();
	obs_data_set_string(event, "eventType", "autoExtendCols");
	obs_data_set_int(event, "value", cols);
	auto handler = obs_source_get_signal_handler(source);
	struct calldata cd;
	uint8_t stack[128];
	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_ptr(&cd, "source", source);
	calldata_set_ptr(&cd, "event", event);
	signal_handler_signal(handler, "signal_event", &cd);
	obs_data_release(event);
}

static const char *gifttv_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("gifttv");
}

static void gifttv_source_update(void *data, obs_data_t *settings)
{
	GiftTV *s = (GiftTV *)data;
	quint32 row = obs_data_get_int(settings, "row");
	quint32 col = obs_data_get_int(settings, "col");

	if (s->needExtendCols(col)) {
		int oldCols = s->currentCols();
		int oldRows = s->currentRows();
		obs_data_set_int(settings, "row", oldRows);
		obs_data_set_int(settings, "col", oldCols);

		row = oldRows;
		col = oldCols;
	}

	obs_data_set_int(settings, "width", col * 456 + (col - 1) * 20);
	obs_data_set_int(settings, "height", row * 120);

	quint32 triggerCondition =
		obs_data_get_int(settings, "triggerCondition");
	s->setTriggerCondition(triggerCondition);

	quint32 triggerConditionValue =
		obs_data_get_int(settings, "triggerConditionValue");
	s->setTriggerConditionValue(triggerConditionValue);

	int prefer = obs_data_get_int(settings, "prefer");
	int mode = obs_data_get_int(settings, "mode");
	int disappear = obs_data_get_int(settings, "disappear");
	s->updateDisappear(disappear);
	s->updatePrefer(prefer);
	s->updateMode(mode);
	s->baseUpdate(settings);
	s->refurshGrid(row, col);

	QStringList giftArray;
	obs_data_array_t *array = obs_data_get_array(settings, "giftArray");
	int count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *item = obs_data_array_item(array, i);
		const char *gift = obs_data_get_string(item, "value");
		giftArray.append(gift);
		obs_data_release(item);
	}
	obs_data_array_release(array);
	s->loadGiftArray(giftArray);

	QString gift = obs_data_get_string(settings, "gift");
	s->add(gift);

	s->gridPrintf();
}

static void gifttv_source_save(void *data, obs_data_t *settings)
{
	if (!data)
		return;
	GiftTV *s = (GiftTV *)data;

	obs_data_erase(settings, "gift");

	QJsonArray array = s->getGiftListByMap();

	obs_data_array *ret = obs_data_array_create();
	if (array.size() && s->mode() == 0 && s->disappear() < 2) {
		for (int i = 0; i < array.size(); i++) {
			auto obj = array.at(i).toObject();
			obs_data_t *item = obs_data_create();
			obs_data_set_string(item, "value",
					    QJsonDocument(obj).toJson(
						    QJsonDocument::Compact));
			obs_data_array_push_back(ret, item);
			obs_data_release(item);
		}
	}
	obs_data_set_array(settings, "giftArray", ret);
	obs_data_array_release(ret);
}

static void *gifttv_source_create(obs_data_t *settings, obs_source_t *source)
{
	GiftTV *context = new GiftTV();
	context->setSource(source);

	gifttv_source_update(context, settings);
	return context;
}

static void gifttv_source_destroy(void *data)
{
	if (!data)
		return;
	GiftTV *s = (GiftTV *)data;
	s->stopTimer();
	s->baseDestroy();
	s->deleteLater();
}

static void gifttv_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 932);
	obs_data_set_default_int(settings, "height", 600);

	obs_data_set_default_int(settings, "triggerCondition", 0);
	obs_data_set_default_int(settings, "triggerConditionValue", 1000);
	obs_data_set_default_int(settings, "row", 5);
	obs_data_set_default_int(settings, "col", 2);
	obs_data_set_default_int(settings, "prefer", 0);
	obs_data_set_default_int(settings, "mode", 0);
	obs_data_set_default_int(settings, "disappear", 0);

	GiftTV::default(settings);
}

static obs_properties_t *gifttv_source_properties(void *data)
{
	if (!data)
		return nullptr;
	GiftTV *s = (GiftTV *)data;
	obs_properties_t *props = s->baseProperties();

	obs_properties_add_int(props, "triggerCondition", u8"触发条件", 0, 1,
			       1);
	obs_properties_add_int(props, "triggerConditionValue", u8"单价设定",
			       100, 999999, 100);
	obs_properties_add_int(props, "row", u8"行", 1, 8, 1);
	obs_properties_add_int(props, "col", u8"列", 1, 4, 1);
	obs_properties_add_int(props, "prefer", u8"优先展示", 0, 1, 1);
	obs_properties_add_int(props, "mode", u8"展示模式", 0, 2, 1);
	obs_properties_add_int(props, "disappear", u8"消失设定", 0, 2, 1);
	return props;
}

struct obs_source_info gifttv_source_info = {
	"gifttv_source",
	OBS_SOURCE_TYPE_INPUT,
	OBS_SOURCE_VIDEO | OBS_SOURCE_INTERACTION | OBS_SOURCE_DO_NOT_DUPLICATE,
	gifttv_source_get_name,
	gifttv_source_create,
	gifttv_source_destroy,
	base_source_getwidth,
	base_source_getheight,
	gifttv_source_defaults,
	gifttv_source_properties,
	gifttv_source_update,
	nullptr,
	nullptr,
	base_source_show,
	base_source_hide,
	nullptr,
	base_source_render,
	nullptr,
	nullptr,
	nullptr,
	gifttv_source_save,
	nullptr,
	base_source_mouse_click,
	base_source_mouse_move,
	base_source_mouse_wheel,
	base_source_focus,
	base_source_key_click,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr};
