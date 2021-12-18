#include "notelistdelegate.h"
#include <QPainter>
#include <QEvent>
#include <QDebug>
#include <QApplication>
#include <QFontDatabase>
#include <QtMath>
#include <QPainterPath>
#include "notelistmodel.h"
#include "noteeditorlogic.h"
#include "tagpool.h"
#include "nodepath.h"
#include "notelistdelegateeditor.h"

NoteListDelegate::NoteListDelegate(NoteListView *view, TagPool *tagPool, QObject *parent)
    : QStyledItemDelegate(parent),
      m_view{view},
      m_tagPool{tagPool},
      #ifdef __APPLE__
      m_displayFont(QFont(QStringLiteral("SF Pro Text")).exactMatch() ? QStringLiteral("SF Pro Text") : QStringLiteral("Roboto")),
      #elif _WIN32
      m_displayFont(QFont(QStringLiteral("Segoe UI")).exactMatch() ? QStringLiteral("Segoe UI") : QStringLiteral("Roboto")),
      #else
      m_displayFont(QStringLiteral("Roboto")),
      #endif

      #ifdef __APPLE__
      m_titleFont(m_displayFont, 13, 65),
      m_titleSelectedFont(m_displayFont, 13),
      m_dateFont(m_displayFont, 13),
      #else
      m_titleFont(m_displayFont, 10, 60),
      m_titleSelectedFont(m_displayFont, 10),
      m_dateFont(m_displayFont, 10),
      #endif
      m_titleColor(26, 26, 26),
      m_dateColor(26, 26, 26),
      m_contentColor(142, 146, 150),
      m_ActiveColor(218, 233, 239),
      m_notActiveColor(175, 212, 228),
      m_hoverColor(207, 207, 207),
      m_applicationInactiveColor(207, 207, 207),
      m_separatorColor(191, 191, 191),
      m_defaultColor(247, 247, 247),
      m_rowHeight(106),
      m_maxFrame(200),
      m_rowRightOffset(0),
      m_state(Normal),
      m_isActive(false),
      m_isInAllNotes(false),
      m_theme(Theme::Light)
{
    m_timeLine = new QTimeLine(300, this);
    m_timeLine->setFrameRange(0,m_maxFrame);
    m_timeLine->setUpdateInterval(10);
    m_timeLine->setCurveShape(QTimeLine::EaseInCurve);
    m_folderIcon = QImage(":/images/folder.png");
    connect( m_timeLine, &QTimeLine::frameChanged, this, [this](){
        emit sizeHintChanged(m_animatedIndex);
    });

    connect(m_timeLine, &QTimeLine::finished, this, [this](){
        m_view->openPersistentEditorC(m_animatedIndex);
        m_animatedIndex = QModelIndex();
        m_state = Normal;
    });
}

void NoteListDelegate::setState(States NewState, QModelIndex index)
{
    m_animatedIndex = index;

    auto startAnimation = [this](QTimeLine::Direction diretion, int duration){
        m_view->closePersistentEditorC(m_animatedIndex);
        m_timeLine->setDirection(diretion);
        m_timeLine->setDuration(duration);
        m_timeLine->start();
    };

    switch ( NewState ){
    case Insert:
        startAnimation(QTimeLine::Forward, m_maxFrame);
        break;
    case Remove:
        startAnimation(QTimeLine::Backward, m_maxFrame);
        break;
    case MoveOut:
        startAnimation(QTimeLine::Backward, m_maxFrame);
        break;
    case MoveIn:
        startAnimation(QTimeLine::Backward, m_maxFrame);
        break;
    case Normal:
        m_animatedIndex = QModelIndex();
        break;
    }

    m_state = NewState;
}

void NoteListDelegate::setAnimationDuration(const int duration)
{
    m_timeLine->setDuration(duration);
}

void NoteListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    bool isHaveTags = index.data(NoteListModel::NoteTagsList).value<QSet<int>>().size() > 0;
    if(index != m_animatedIndex && isHaveTags){
        return;
    }
    painter->setRenderHint(QPainter::Antialiasing);
    QStyleOptionViewItem opt = option;
    opt.rect.setWidth(option.rect.width() - m_rowRightOffset);

    int currentFrame = m_timeLine->currentFrame();
    double rate = (currentFrame/(m_maxFrame * 1.0));
    double height = m_rowHeight * rate;

    switch(m_state){
    case Insert:
    case Remove:
    case MoveOut:
        if(index == m_animatedIndex){
            opt.rect.setHeight(int(height));
            opt.backgroundBrush.setColor(m_notActiveColor);
        }
        break;
    case MoveIn:
        if(index == m_animatedIndex){
            opt.rect.setY(int(height));
        }
        break;
    case Normal:
        break;
    }

    paintBackground(painter, opt, index);
    paintLabels(painter, option, index);
}

QSize NoteListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize result = QStyledItemDelegate::sizeHint(option, index);
    auto id = index.data(NoteListModel::NoteID).toInt();
    bool isHaveTags = index.data(NoteListModel::NoteTagsList).value<QSet<int>>().size() > 0;
#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    if (index != m_animatedIndex && isHaveTags) {
#else
    if (m_view->isPersistentEditorOpen(index) && index != m_animatedIndex && isHaveTags) {
#endif
        if (szMap.contains(id)) {
            result.setHeight(szMap[id].height());
            return result;
        }
    }
    int rowHeight = 70;
    if (isHaveTags) {
        rowHeight = m_rowHeight;
    }
    if(index == m_animatedIndex){
        if(m_state == MoveIn){
            result.setHeight(rowHeight);
        }else{
            double rate = m_timeLine->currentFrame()/(m_maxFrame * 1.0);
            double height = rowHeight * rate;
            result.setHeight(int(height));
        }
    } else {
        result.setHeight(rowHeight);
    }
    if (m_isInAllNotes) {
        result.setHeight(result.height() + 20);
    }
    return result;
}

QTimeLine::State NoteListDelegate::animationState()
{
    return m_timeLine->state();
}

void NoteListDelegate::paintBackground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if((option.state & QStyle::State_Selected) == QStyle::State_Selected){
        if(qApp->applicationState() == Qt::ApplicationActive){
            if(m_isActive){
                painter->fillRect(option.rect, QBrush(m_ActiveColor));
            }else{
                painter->fillRect(option.rect, QBrush(m_notActiveColor));
            }
        }else if(qApp->applicationState() == Qt::ApplicationInactive){
            painter->fillRect(option.rect, QBrush(m_applicationInactiveColor));
        }
    }else if((option.state & QStyle::State_MouseOver) == QStyle::State_MouseOver){
        painter->fillRect(option.rect, QBrush(m_hoverColor));
    }else if((index.row() !=  m_currentSelectedIndex.row() - 1)
             && (index.row() !=  m_hoveredIndex.row() - 1)){

        painter->fillRect(option.rect, QBrush(m_defaultColor));
        paintSeparator(painter, option, index);
    }
}

void NoteListDelegate::paintLabels(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QString title{index.data(NoteListModel::NoteFullTitle).toString()};
    QFont titleFont = (option.state & QStyle::State_Selected) == QStyle::State_Selected ? m_titleSelectedFont : m_titleFont;
    QFontMetrics fmTitle(titleFont);
    QRect fmRectTitle = fmTitle.boundingRect(title);

    QString date = parseDateTime(index.data(NoteListModel::NoteLastModificationDateTime).toDateTime());
    QFontMetrics fmDate(m_dateFont);
    QRect fmRectDate = fmDate.boundingRect(date);

    QString parentName{index.data(NoteListModel::NoteParentName).toString()};
    QFontMetrics fmParentName(titleFont);
    QRect fmRectParentName = fmParentName.boundingRect(parentName);

    QString content{index.data(NoteListModel::NoteContent).toString()};
    content = NoteEditorLogic::getSecondLine(content);
    QFontMetrics fmContent(titleFont);
    QRect fmRectContent = fmContent.boundingRect(content);

    double rowPosX = option.rect.x();
    double rowPosY = option.rect.y();
    double rowWidth = option.rect.width();

    double titleRectPosX = rowPosX + NoteListConstant::leftOffsetX;
    double titleRectPosY = rowPosY;
    double titleRectWidth = rowWidth - 2.0 * NoteListConstant::leftOffsetX;
    double titleRectHeight = fmRectTitle.height() + NoteListConstant::topOffsetY;

    double dateRectPosX = rowPosX + NoteListConstant::leftOffsetX;
    double dateRectPosY = rowPosY + fmRectTitle.height() + NoteListConstant::topOffsetY;
    double dateRectWidth = rowWidth - 2.0 * NoteListConstant::leftOffsetX;
    double dateRectHeight = fmRectDate.height() + NoteListConstant::titleDateSpace;

    double contentRectPosX = rowPosX + NoteListConstant::leftOffsetX;
    double contentRectPosY = rowPosY + fmRectTitle.height() + fmRectDate.height() + NoteListConstant::topOffsetY;
    double contentRectWidth = rowWidth - 2.0 * NoteListConstant::leftOffsetX;
    double contentRectHeight = fmRectContent.height() + NoteListConstant::dateDescSpace;

    double folderNameRectPosX = 0;
    double folderNameRectPosY = 0;
    double folderNameRectWidth = 0;
    double folderNameRectHeight = 0;

    if (m_isInAllNotes) {
        folderNameRectPosX = rowPosX + NoteListConstant::leftOffsetX + 20;
        folderNameRectPosY = rowPosY + fmRectContent.height() + fmRectTitle.height() + fmRectDate.height() + NoteListConstant::topOffsetY;
        folderNameRectWidth = rowWidth - 2.0 * NoteListConstant::leftOffsetX;
        folderNameRectHeight = fmRectParentName.height() + NoteListConstant::descFolderSpace;
    }

    double rowRate = m_timeLine->currentFrame()/(m_maxFrame * 1.0);
    double currRowHeight = m_rowHeight * rowRate;


    auto drawStr = [painter](double posX, double posY, double width, double height, QColor color, QFont font, QString str){
        QRectF rect(posX, posY, width, height);
        painter->setPen(color);
        painter->setFont(font);
        painter->drawText(rect, Qt::AlignBottom, str);
    };

    // set the bounding Rect of title and date string
    if(index.row() == m_animatedIndex.row()){
        if(m_state == MoveIn){
            titleRectHeight = NoteListConstant::topOffsetY + fmRectTitle.height() + currRowHeight;
            dateRectPosY = titleRectHeight;
            dateRectHeight = fmRectDate.height() + NoteListConstant::titleDateSpace;
            if (m_isInAllNotes) {
                contentRectPosY = fmParentName.height() + NoteListConstant::titleDateSpace;
                folderNameRectPosY = contentRectPosY + dateRectPosY + dateRectHeight + 5;
            } else {
                contentRectPosY = dateRectPosY + dateRectHeight;
            }
        }else{
            if((fmRectTitle.height() + NoteListConstant::topOffsetY) >= ((1.0 - rowRate) * m_rowHeight)){
                titleRectHeight = (fmRectTitle.height() + NoteListConstant::topOffsetY) - (1.0 - rowRate) * m_rowHeight;
            }else{
                titleRectHeight = 0;

                double labelsSumHeight = fmRectTitle.height() + NoteListConstant::topOffsetY
                        + fmRectDate.height() + NoteListConstant::titleDateSpace;
                double bottomSpace = m_rowHeight - labelsSumHeight;

                if(currRowHeight > bottomSpace){
                    dateRectHeight = currRowHeight - bottomSpace;
                }else{
                    dateRectHeight = 0;
                }
            }

            dateRectPosY = titleRectHeight + rowPosY;
            if (m_isInAllNotes) {
                contentRectPosY = fmParentName.height() + NoteListConstant::titleDateSpace;
                folderNameRectPosY = contentRectPosY + dateRectPosY + dateRectHeight + rowPosY + 5;
            } else {
                contentRectPosY = dateRectPosY + dateRectHeight + rowPosY;
            }
        }
    }

//    double tagsPosY = contentRectPosY + fmRectParentName.height() + fmRectContent.height() + topOffsetY + 10;

    // draw title & date
    title = fmTitle.elidedText(title, Qt::ElideRight, int(titleRectWidth));
    content = fmContent.elidedText(content, Qt::ElideRight, int(titleRectWidth));
    drawStr(titleRectPosX, titleRectPosY, titleRectWidth, titleRectHeight, m_titleColor, titleFont, title);
    drawStr(dateRectPosX, dateRectPosY, dateRectWidth, dateRectHeight, m_dateColor, m_dateFont, date);
    if (m_isInAllNotes) {
        painter->drawImage(QRect(rowPosX + NoteListConstant::leftOffsetX,
                                 folderNameRectPosY + NoteListConstant::descFolderSpace,
                                 16, 16), m_folderIcon);
        drawStr(folderNameRectPosX, folderNameRectPosY, folderNameRectWidth, folderNameRectHeight, m_contentColor, titleFont, parentName);
    }
    drawStr(contentRectPosX, contentRectPosY, contentRectWidth, contentRectHeight, m_contentColor, titleFont, content);
//    paintTagList(tagsPosY, painter, option, index);
}

void NoteListDelegate::paintSeparator(QPainter*painter, const QStyleOptionViewItem&option, const QModelIndex&index) const
{
    Q_UNUSED(index)

    painter->setPen(QPen(m_separatorColor));
    const int leftOffsetX = 11;
    int posX1 = option.rect.x() + leftOffsetX;
    int posX2 = option.rect.x() + option.rect.width() - leftOffsetX - 1;
    int posY = option.rect.y() + option.rect.height() - 1;

    painter->drawLine(QPoint(posX1, posY),
                      QPoint(posX2, posY));
}

void NoteListDelegate::paintTagList(int top, QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    auto tagIds = index.data(NoteListModel::NoteTagsList).value<QSet<int>>();
    int left = option.rect.x() + 10;

    for (const auto& id : QT_AS_CONST(tagIds)) {
        if (left >= option.rect.width()) {
            break;
        }
        auto tag = m_tagPool->getTag(id);
        QFontMetrics fmName(m_titleFont);
        QRect fmRectName = fmName.boundingRect(tag.name());

        auto rect = option.rect;
        rect.setTop(top);
        rect.setLeft(left);
        rect.setHeight(20);
        rect.setWidth(5 + 12 + 5 + fmRectName.width() + 7);
        left += rect.width() + 5;
        QPainterPath path;
        path.addRoundedRect(rect, 10, 10);
        if (m_theme == Theme::Dark) {
            painter->fillPath(path, QColor(76, 85, 97));
        } else if((option.state & QStyle::State_Selected) == QStyle::State_Selected) {
            painter->fillPath(path, QColor(218, 235, 248));
        } else {
            painter->fillPath(path, QColor(227, 234, 243));
        }
        auto iconRect = QRect(rect.x() + 5, rect.y() + (rect.height() - 12) / 2, 12, 12);
        painter->setBrush(QColor(tag.color()));
        painter->setPen(QColor(tag.color()));
        painter->drawEllipse(iconRect);
        painter->setBrush(m_titleColor);
        painter->setPen(m_titleColor);

        QRect nameRect(rect);
        nameRect.setLeft(iconRect.x() + iconRect.width() + 5);
        painter->setFont(m_titleFont);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, tag.name());
    }
}

QString NoteListDelegate::parseDateTime(const QDateTime &dateTime) const
{
    QLocale usLocale(QLocale("en_US"));

    auto currDateTime = QDateTime::currentDateTime();

    if(dateTime.date() == currDateTime.date()){
        return usLocale.toString(dateTime.time(),"h:mm A");
    }else if(dateTime.daysTo(currDateTime) == 1){
        return "Yesterday";
    }else if(dateTime.daysTo(currDateTime) >= 2 &&
             dateTime.daysTo(currDateTime) <= 7){
        return usLocale.toString(dateTime.date(), "dddd");
    }

    return dateTime.date().toString("M/d/yy");
}

const QModelIndex &NoteListDelegate::hoveredIndex() const
{
    return m_hoveredIndex;
}

const QModelIndex &NoteListDelegate::currentSelectedIndex() const
{
    return m_currentSelectedIndex;
}

bool NoteListDelegate::isInAllNotes() const
{
    return m_isInAllNotes;
}

Theme NoteListDelegate::theme() const
{
    return m_theme;
}

void NoteListDelegate::setIsInAllNotes(bool newIsInAllNotes)
{
    m_isInAllNotes = newIsInAllNotes;
}

void NoteListDelegate::clearSizeMap()
{
    szMap.clear();
}

void NoteListDelegate::updateSizeMap(int id, const QSize &sz, const QModelIndex &index)
{
    szMap[id] = sz;
    emit sizeHintChanged(index);
}

void NoteListDelegate::editorDestroyed(int id, const QModelIndex &index)
{
    szMap.remove(id);
    emit sizeHintChanged(index);
}

QWidget *NoteListDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid()) {
        return nullptr;
    }
    bool isHaveTags = index.data(NoteListModel::NoteTagsList).value<QSet<int>>().size() > 0;
    if (!isHaveTags) {
        return nullptr;
    }
    auto w = new NoteListDelegateEditor(this, m_view, option, index, m_tagPool, parent);
    w->setTheme(m_theme);
    connect(this, &NoteListDelegate::themeChanged,
            w, &NoteListDelegateEditor::setTheme);
    connect(w, &NoteListDelegateEditor::updateSizeHint,
            this, &NoteListDelegate::updateSizeMap);
    connect(w, &NoteListDelegateEditor::nearDestroyed,
            this, &NoteListDelegate::editorDestroyed);
    w->recalculateSize();
    return w;
}

void NoteListDelegate::setActive(bool isActive)
{
    m_isActive = isActive;
}

void NoteListDelegate::setRowRightOffset(int rowRightOffset)
{
    m_rowRightOffset = rowRightOffset;
}

void NoteListDelegate::setHoveredIndex(const QModelIndex &hoveredIndex)
{
    m_hoveredIndex = hoveredIndex;
}

void NoteListDelegate::setCurrentSelectedIndex(const QModelIndex &currentSelectedIndex)
{
    m_currentSelectedIndex = currentSelectedIndex;
}

void NoteListDelegate::setTheme(Theme theme)
{
    m_theme = theme;
    switch(m_theme){
    case Theme::Light:
    {
        m_titleColor = QColor(26, 26, 26);
        m_dateColor = QColor(26, 26, 26);
        m_defaultColor = QColor(247, 247, 247);
        m_ActiveColor = QColor(218, 233, 239);
        m_notActiveColor = QColor(175, 212, 228);
        m_hoverColor = QColor(207, 207, 207);
        break;
    }
    case Theme::Dark:
    {
        m_titleColor = QColor(204, 204, 204);
        m_dateColor = QColor(204, 204, 204);
        m_defaultColor = QColor(26, 26, 26);
        m_ActiveColor = QColor(0, 59, 148);
        m_notActiveColor = QColor(0, 59, 148);
        m_hoverColor = QColor(15, 45, 90);
        break;
    }
    case Theme::Sepia:
    {
        m_titleColor = QColor(26, 26, 26);
        m_dateColor = QColor(26, 26, 26);
        m_defaultColor = QColor(251, 240, 217);
        m_ActiveColor = QColor(218, 233, 239);
        m_notActiveColor = QColor(175, 212, 228);
        m_hoverColor = QColor(207, 207, 207);
        break;
    }
    }
    emit themeChanged(m_theme);
}
