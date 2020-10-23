#include "gift-tv.h"
#include "renderer.h"
#include <QDebug>

int gridWidth = 456;
int gridHeight = 120;

std::string* grid[];
std::string Placeholder = "Placeholder";

struct GiftInfo {
	int x;
	int y;
	int giftId;
	int giftType; //0:普通 1:大礼物
	int count;
	int hitCount;
	quint32 price;
	std::string giftPath;
	std::string gfitName;
	std::string name;
	std::string recName;
	std::string avatarPath;
};

//static void UpdateGiftPosition(obs_data_t *settings, GiftInfo gift) {
//	int rows = obs_data_get_int(settings, "row");
//	int cols = obs_data_get_int(settings, "col");
//	int prefer = obs_data_get_int(settings, "prefer");
//
//	bool find = false;
//	for (int row = 0; row < rows; ++row)
//	{
//		for (int col = 0; col < cols; ++col)
//		{
//			std::string giftContent = grid[row][col];
//			// 若为空，表示栅格位置空闲
//			if (giftContent == "")
//			{
//				// 若为小礼物，则只需要一个单元格，故直接计算坐标点
//				if (gift.giftType == 0)
//				{
//					gift.x = col * (gridWidth + 20);
//					gift.y = row * gridHeight + 10;
//					grid[row][col] = gift.name;
//					find = true;
//					break;
//				}
//
//				// 若为大礼物，则需要两个单元格，故继续判断下一个水平单元格是否为空，并计算坐标点
//				// 否则继续查找可以放下大礼物的位置
//				if (col + 1 < cols && grid[row][col + 1] == "")
//				{
//					gift.x = col * (gridWidth + 20);
//					gift.y = row * gridHeight;
//					grid[row][col] = gift.name;
//					grid[row][col + 1] = Placeholder;
//					find = true;
//					break;
//				}
//			}
//		}
//	}
//
//	if (find)
//		return;
//
//	// 若没有找到，则先查找价值最低的礼物，并记录当前栅格中空闲单元格数量
//	int min = 0;
//	int tempRow = 0;
//	int tempCol = 0;
//	int emptyGrid = 0;
//	if (!find)
//	{
//		for (int row = 0; row < rows; ++row) {
//			for (int col = 0; col < cols; ++col) {
//				std::string giftContent = grid[row][col];
//				if (giftContent == "")
//					emptyGrid++;
//				if (giftContent == Placeholder)
//					continue;
//				if ( < min)
//				{
//					tempRow = row;
//					tempCol = col;
//				}
//			}
//		}
//	}
//
//	// 淘汰查找出的价值最低的礼物，如果淘汰的礼物是大礼物时，该礼物占位的单元格状态都需要维护
//	// todo
//
//	// 显示新增的礼物
//	if (gift.giftType == 0)
//	{
//		// 如果需要插入的礼物为普通礼物
//		gift.x = tempCol * (gridWidth + 20);
//		gift.y = tempRow * gridHeight + 10;
//		grid[tempRow][tempCol] = gift.name;
//	}
//	else {
//		// 如果需要插入的礼物为大礼物
//		gift.x = tempCol * (gridWidth + 20);
//		gift.y = tempRow * gridHeight;
//		grid[tempRow][tempCol] = gift.name;
//		grid[tempRow][tempCol + 1] = Placeholder;
//	}
//}

GiftTV::GiftTV(QObject *parent /* = nullptr */) : QmlSourceBase(parent)
{
	addProperties("gifttvProperties", this);
}

void GiftTV::default(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "file",
				    "qrc:/qmlfiles/GiftTV.qml");
}

static const char *gifttv_source_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("gifttv");
}

static void gifttv_source_update(void *data, obs_data_t *settings)
{
	GiftTV *s = (GiftTV *)data;
	s->baseUpdate(settings);

	quint32 prefer = obs_data_get_int(settings, "prefer");
	s->setprefer((GiftTV::PreferLayout)prefer);
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
	s->baseDestroy();
	s->deleteLater();
}

static void gifttv_source_defaults(obs_data_t *settings)
{
	QmlSourceBase::baseDefault(settings);
	obs_data_set_default_int(settings, "width", 932);
	obs_data_set_default_int(settings, "height", 600);

	obs_data_set_default_int(settings, "triggerCondition", 1);
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
	obs_properties_add_int(props, "triggerConditionValue", u8"单价设定", 0,
			       999999, 1000);
	obs_properties_add_int(props, "row", u8"行", 1, 9, 1);
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
	nullptr,
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
