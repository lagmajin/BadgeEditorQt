module;

export module badge.event;

import std;

export namespace badge {

enum class AppEventKind {
    BadgeMoved,
    BadgeEdited,
    LayoutDirty,
    DiagnosticsDirty,
};

struct AppEvent {
    AppEventKind kind = AppEventKind::LayoutDirty;
    int badgeIndex = -1;
    double xMm = 0.0;
    double yMm = 0.0;
    std::vector<int> selectedIndices;
    std::string reason;
};

class AppEventQueue {
public:
    void post(AppEvent event) {
        coalesceAndAppend(std::move(event));
    }

    void postBadgeMoved(int badgeIndex, double xMm, double yMm) {
        AppEvent event;
        event.kind = AppEventKind::BadgeMoved;
        event.badgeIndex = badgeIndex;
        event.xMm = xMm;
        event.yMm = yMm;
        coalesceAndAppend(std::move(event));
    }

    void postDirty(AppEventKind kind, std::string reason = {}) {
        AppEvent event;
        event.kind = kind;
        event.reason = std::move(reason);
        coalesceAndAppend(std::move(event));
    }

    void postBadgeEdited(std::string reason = {}) {
        postDirty(AppEventKind::BadgeEdited, std::move(reason));
    }

    [[nodiscard]] bool empty() const noexcept {
        return m_events.empty();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return m_events.size();
    }

    [[nodiscard]] bool containsKind(AppEventKind kind) const noexcept {
        for (const auto& event : m_events) {
            if (event.kind == kind) {
                return true;
            }
        }
        return false;
    }

    void clear() noexcept {
        m_events.clear();
    }

    std::vector<AppEvent> drain() {
        std::vector<AppEvent> out;
        out.swap(m_events);
        return out;
    }

private:
    std::vector<AppEvent> m_events;

    static bool isReplaceableKind(AppEventKind kind) {
        switch (kind) {
        case AppEventKind::BadgeMoved:
        case AppEventKind::BadgeEdited:
        case AppEventKind::LayoutDirty:
        case AppEventKind::DiagnosticsDirty:
            return true;
        }
        return false;
    }

    void coalesceAndAppend(AppEvent event) {
        if (!isReplaceableKind(event.kind)) {
            m_events.push_back(std::move(event));
            return;
        }

        if (event.kind == AppEventKind::BadgeMoved && event.badgeIndex >= 0) {
            for (auto& existing : m_events) {
                if (existing.kind == AppEventKind::BadgeMoved && existing.badgeIndex == event.badgeIndex) {
                    existing = std::move(event);
                    return;
                }
            }
        } else {
            for (auto it = m_events.rbegin(); it != m_events.rend(); ++it) {
                if (it->kind == event.kind) {
                    *it = std::move(event);
                    return;
                }
            }
        }

        m_events.push_back(std::move(event));
    }
};

inline AppEvent makeBadgeMovedEvent(int badgeIndex, double xMm, double yMm) {
    AppEvent event;
    event.kind = AppEventKind::BadgeMoved;
    event.badgeIndex = badgeIndex;
    event.xMm = xMm;
    event.yMm = yMm;
    return event;
}

inline AppEvent makeDirtyEvent(AppEventKind kind, std::string reason = {}) {
    AppEvent event;
    event.kind = kind;
    event.reason = std::move(reason);
    return event;
}

} // namespace badge
