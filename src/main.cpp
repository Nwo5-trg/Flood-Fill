#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>

using namespace geode::prelude;

// we trusting this flag with *everything*
bool floodFilling = false;

class $modify(EditorUIHook, EditorUI) {
    struct Fields {
        // fuck you im not making an enum in a 1 file mod
        int notificationMode;
        bool diagonal;
        GameObject* floodFillCenter;
    };

    void onFill(CCObject* sender) {
        auto objs = getSelectedObjects();
        if (objs->count() < 1) {
            showNotification("u need to select atleast *a* object yk nso, uhh, go select them :3", true);
            return;
        }
    
        if (objs->count() == 2) {
            floodFilling = false;
            rectFill(objs);
            deselectAll();
        }
        else if (objs->count() == 1) {
            showNotification("okie now select the outline (u dont have to deselect the center)", false);
            m_fields->floodFillCenter = static_cast<GameObject*>(objs->firstObject());
            floodFilling = true;
            return;
        }
        else if (floodFilling) {
            floodFilling = false;
            floodFill(m_fields->floodFillCenter, objs);
            deselectAll();
        }
        else {
            showNotification("nothing ever happens :3c (u selected too much)", true);
        }

        updateButtons();
        updateObjectInfoLabel();
    }

    void floodFill(GameObject* center, CCArray* objs) {
        auto objString = copyObjects(CCArray::createWithObject(center), true, true);
        float gridSize = ObjectToolbox::sharedState()->gridNodeSizeForKey(center->m_objectID);

        auto min = ccp(FLT_MAX, FLT_MAX);
        auto max = ccp(-FLT_MAX, -FLT_MAX);

        std::vector<CCRect> boundingBoxes;
        for (auto obj : CCArrayExt<GameObject*>(objs)) {
            auto rect = getObjectGridRect(obj);

            boundingBoxes.push_back(rect);

            if (rect.origin.x < min.x) min.x = rect.origin.x;
            if (rect.origin.y < min.y) min.y = rect.origin.y;
            
            auto corner = rect.origin + rect.size;
            if (corner.x > max.x) max.x = corner.x;
            if (corner.y > max.y) max.y = corner.y;
        }

        std::vector<GameObject*> queue { center };
        auto placedObjs = CCArray::create();

        auto createNeighbour = [&, this] (GameObject* obj, CCPoint off) -> GameObject* {
            auto pos = obj->getPosition() + off;

            if (pos.x < min.x || pos.y < min.y || pos.x > max.x || pos.y > max.y) return nullptr;

            for (const auto& box : boundingBoxes) {
                if (box.containsPoint(pos)) return nullptr;
            }

            auto neighbour = static_cast<GameObject*>(pasteObjects(objString, true, true)->firstObject());
            moveObject(neighbour, pos - neighbour->getPosition());
            placedObjs->addObject(neighbour);

            boundingBoxes.push_back(getObjectGridRect(neighbour));

            return neighbour;
        };

        bool diagonal = m_fields->diagonal;
        while (!queue.empty()) {
            auto obj = queue.back();
            queue.pop_back();

            // :3
            if (auto neighbour = createNeighbour(obj, {gridSize, 0.0f})) queue.push_back(neighbour);
            if (auto neighbour = createNeighbour(obj, {0.0f, gridSize})) queue.push_back(neighbour);
            if (auto neighbour = createNeighbour(obj, {-gridSize, 0.0f})) queue.push_back(neighbour);
            if (auto neighbour = createNeighbour(obj, {0.0f, -gridSize})) queue.push_back(neighbour);
            if (diagonal) {
                if (auto neighbour = createNeighbour(obj, {gridSize, gridSize})) queue.push_back(neighbour);
                if (auto neighbour = createNeighbour(obj, {-gridSize, gridSize})) queue.push_back(neighbour);
                if (auto neighbour = createNeighbour(obj, {-gridSize, -gridSize})) queue.push_back(neighbour);
                if (auto neighbour = createNeighbour(obj, {gridSize, -gridSize})) queue.push_back(neighbour);
            }
        }

        m_editorLayer->addToUndoList(UndoObject::createWithArray(placedObjs, UndoCommand::Paste), true);

        showNotification("successfully flood filled :3c", false);
    }

    void rectFill(CCArray* objs) {
        auto p1 = static_cast<GameObject*>(objs->firstObject());
        auto p2 = static_cast<GameObject*>(objs->objectAtIndex(1));
        auto p1Pos = p1->getPosition();
        auto p2Pos = p2->getPosition();

        if (p1Pos == p2Pos) {
            showNotification("both objects r at the same pos sooooo, idk :3c", true);
            return;
        }

        auto objString = copyObjects(CCArray::createWithObject(p1), true, true);
        float gridSize = ObjectToolbox::sharedState()->gridNodeSizeForKey(p1->m_objectID);

        auto min = ccp(std::min(p1Pos.x, p2Pos.x), std::min(p1Pos.y, p2Pos.y));
        auto max = ccp(std::max(p1Pos.x, p2Pos.x), std::max(p1Pos.y, p2Pos.y));

        auto placedObjs = CCArray::create();

        for (float x = min.x; x <= max.x; x += gridSize) {
            for (float y = min.y; y <= max.y; y += gridSize) {
                auto obj = static_cast<GameObject*>(pasteObjects(objString, true, true)->firstObject());
                moveObject(obj, ccp(x, y) - obj->getPosition());
                placedObjs->addObject(obj);
            }
        }

        m_editorLayer->addToUndoList(UndoObject::createWithArray(placedObjs, UndoCommand::Paste), true);
        m_editorLayer->addToUndoList(UndoObject::createWithArray(objs, UndoCommand::DeleteMulti), true);
        
        m_editorLayer->removeObject(p1, true);
        m_editorLayer->removeObject(p2, true);

        showNotification("successfully filled rect :3c", false);
    }

    CCRect getObjectGridRect(GameObject* obj) {
        float size = ObjectToolbox::sharedState()->gridNodeSizeForKey(obj->m_objectID);

        auto boxSize = CCSize(size * obj->m_scaleX, size * obj->m_scaleY);
        auto origin = obj->getPosition() - (boxSize / 2);

        return {origin, boxSize};
    }

    void showNotification(std::string text, bool error) {
        int mode = m_fields->notificationMode;
        if (mode == 0) return;
        if (mode == 1 && error) return;
        Notification::create(
            text, error ? NotificationIcon::Error : NotificationIcon::Success
        )->show();
    }

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        floodFilling = false;

        auto mod = Mod::get();

        auto notifications = mod->getSettingValue<std::string>("notifications");
        if (notifications == "None") m_fields->notificationMode = 0;
        else if (notifications == "All") m_fields->notificationMode = 2;
        else m_fields->notificationMode = 1;

        m_fields->diagonal = mod->getSettingValue<bool>("diagonal");

        return true;
    }
    // temp for testing
    void onPlayback(CCObject* sender) {
        onFill(nullptr);
    }
};

// hopefully will stop most crashes
class $modify(LevelEditorLayer) {
    void removeObject(GameObject* p0, bool p1) {
        floodFilling = false;
        LevelEditorLayer::removeObject(p0, p1);
    }
};