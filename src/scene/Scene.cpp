//#include "scene.h"
//
//flecs::world Scene::ecs;
//
//void Scene::Init() {
//}
//
//flecs::entity Scene::CreateEntity(std::string entityName) {
//    return ecs.entity(entityName.c_str());
//}
//
//void Scene::DestroyEntity(flecs::entity entity) {
//    entity.destruct();
//}
//
//void Scene::Update() {
//    ecs.progress();
//}
//
//void Scene::Destroy() {
//    //renderer->Destroy();
//}
//
//void Scene::Render() {
//#if EDITOR
//    editor->Render();
//#endif
//}
//
//std::vector<flecs::entity> Scene::GetEntities() {
//    std::vector<flecs::entity> entities;
//    //static auto q = ecs.query_builder<RectTransformC>()
//    //                .order_by(0, [](flecs::entity_t e1, const void* ptr1, flecs::entity_t e2, const void* ptr2) {
//    //                    return (e1 > e2) - (e1 < e2);
//    //                })
//    //                .build();
//
//    //q.each([&](const flecs::entity& e, RectTransformC& transform) {
//    //    entities.push_back(e);
//    //});
//
//    return entities;
//}
