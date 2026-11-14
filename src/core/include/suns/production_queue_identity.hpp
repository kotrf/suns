#pragma once
#include "suns/turn_processor.hpp"
#include <map>
#include <optional>

namespace suns::detail {
// Transient command provenance, reconstructed from the received queue and orders.
// It is deliberately independent of a host's acceptance of earlier commands.
struct QueueRowOrigin {
    bool initial{};
    std::size_t order{}, ordinal{};
    bool operator==(const QueueRowOrigin&) const = default;
};
struct QueueEditTarget {
    PlanetId colony{};
    std::optional<QueueRowOrigin> source, destination;
    bool after{};
};
class ProductionQueueIdentity {
public:
    explicit ProductionQueueIdentity(const GameState&, const GameState* received = nullptr);
    void synchronize(const GameState&, std::size_t previousOrder);
    QueueEditTarget target(const Order&) const;
    bool bind(Order&, const QueueEditTarget&) const;
    void erased(PlanetId, std::size_t, const std::vector<ProductionItem>&);
    void moved(PlanetId, std::size_t, std::size_t, const std::vector<ProductionItem>&);
    void template_applied(PlanetId, std::size_t order, std::size_t inserted, const std::vector<ProductionItem>&);
private:
    struct Rows {std::vector<ProductionItem> items;std::vector<QueueRowOrigin> origins;};
    std::map<PlanetId,Rows> rows_;
};
}
