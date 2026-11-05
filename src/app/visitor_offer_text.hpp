#pragma once
#include "suns/intergalactic_visitors.hpp"
#include <QStringList>

namespace suns {
inline QString visitor_request_text(VisitorProfile profile)
{
    const auto price = visitor_offer(profile).request;
    QStringList parts;
    if (price.ironium) parts << QString("%1 kt Ironium").arg(price.ironium);
    if (price.boranium) parts << QString("%1 kt Boranium").arg(price.boranium);
    if (price.germanium) parts << QString("%1 kt Germanium").arg(price.germanium);
    return parts.join(" + ");
}
inline QString visitor_gift_text(VisitorProfile profile)
{ return QString::fromStdString(component_spec(visitor_offer(profile).gift).name) + " blueprint"; }
inline QString visitor_offer_text(VisitorProfile profile)
{ return visitor_request_text(profile) + " → " + visitor_gift_text(profile); }
}
