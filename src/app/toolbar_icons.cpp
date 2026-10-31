#include "toolbar_icons.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <cmath>

namespace suns {

QIcon toolbarIcon(ToolbarGlyph glyph)
{
    QIcon icon;
    for (int size : {18, 36, 54}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(size / 24.0, size / 24.0);
        const QColor ink("#bcd4e8");
        p.setPen(QPen(ink, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        const auto line = [&](double x1, double y1, double x2, double y2) {
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
        };
        const auto arrow = [&](QPointF from, QPointF to) {
            p.drawLine(from, to);
            const auto d = to - from;
            const auto n = d / std::hypot(d.x(), d.y());
            const QPointF cross(-n.y(), n.x());
            p.drawLine(to, to - n * 4 + cross * 2.5);
            p.drawLine(to, to - n * 4 - cross * 2.5);
        };
        switch (glyph) {
        case ToolbarGlyph::EndTurn:
            p.setBrush(QColor("#e4b77d"));
            p.setPen(Qt::NoPen);
            p.drawPolygon(QPolygonF{{6, 4}, {20, 12}, {6, 20}});
            break;
        case ToolbarGlyph::Designer:
            p.drawPolygon(QPolygonF{{12, 3}, {19, 19}, {12, 16}, {5, 19}});
            line(12, 7, 12, 14);
            break;
        case ToolbarGlyph::Research: {
            QPainterPath flask;
            flask.moveTo(9, 3); flask.lineTo(9, 9); flask.lineTo(4, 18);
            flask.quadTo(3, 21, 7, 21); flask.lineTo(17, 21);
            flask.quadTo(21, 21, 20, 18); flask.lineTo(15, 9); flask.lineTo(15, 3);
            p.drawPath(flask);
            line(8, 3, 16, 3); line(7, 15, 17, 15);
            break;
        }
        case ToolbarGlyph::Plans:
        case ToolbarGlyph::Orders:
            p.drawRoundedRect(QRectF(5, 4, 14, 17), 1.5, 1.5);
            p.drawRoundedRect(QRectF(9, 2, 6, 4), 1, 1);
            if (glyph == ToolbarGlyph::Orders) {
                line(8, 12, 11, 15); line(11, 15, 16, 9);
            } else {
                line(8, 10, 16, 10); line(8, 14, 16, 14); line(8, 18, 13, 18);
            }
            break;
        case ToolbarGlyph::History:
            line(4, 4, 4, 20); line(4, 20, 21, 20);
            p.drawPolyline(QPolygonF{{7, 16}, {11, 11}, {15, 14}, {20, 5}});
            break;
        case ToolbarGlyph::Fleets:
            for (const QPointF center : {QPointF(7, 16), QPointF(12, 7), QPointF(17, 16)}) {
                p.drawPolygon(QPolygonF{center + QPointF(0, -4), center + QPointF(3, 3), center, center + QPointF(-3, 3)});
            }
            break;
        case ToolbarGlyph::Cargo:
            p.drawPolygon(QPolygonF{{4, 7}, {12, 3}, {20, 7}, {20, 17}, {12, 21}, {4, 17}});
            line(4, 7, 12, 11); line(12, 11, 20, 7); line(12, 11, 12, 21);
            break;
        case ToolbarGlyph::Merge:
            arrow({3, 5}, {11, 12}); arrow({3, 19}, {11, 12}); arrow({12, 12}, {21, 12});
            break;
        case ToolbarGlyph::Split:
            line(3, 12, 10, 12); arrow({10, 12}, {21, 4}); arrow({10, 12}, {21, 20});
            break;
        case ToolbarGlyph::ZoomOut:
        case ToolbarGlyph::ZoomIn:
            p.drawEllipse(QRectF(3, 3, 13, 13)); line(14, 14, 21, 21);
            line(6, 9.5, 13, 9.5);
            if (glyph == ToolbarGlyph::ZoomIn) line(9.5, 6, 9.5, 13);
            break;
        case ToolbarGlyph::Sensors:
            p.drawEllipse(QRectF(3, 3, 18, 18)); p.drawEllipse(QRectF(7, 7, 10, 10));
            line(12, 12, 19, 5); p.setBrush(ink); p.drawEllipse(QPointF(9, 14), 1.5, 1.5);
            break;
        case ToolbarGlyph::Fit:
        case ToolbarGlyph::Fullscreen:
            for (int x : {4, 20}) for (int y : {4, 20}) {
                line(x, y, x == 4 ? 9 : 15, y);
                line(x, y, x, y == 4 ? 9 : 15);
            }
            if (glyph == ToolbarGlyph::Fit) p.drawEllipse(QPointF(12, 12), 3, 3);
            break;
        }
        p.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace suns
