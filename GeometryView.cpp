// Opengl
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/vector_angle.hpp>

#include "imgui.h"
#include "ImGuiToolkit.h"

// memmove
#include <string>
#include <sstream>
#include <iomanip>

#include "Mixer.h"
#include "defines.h"
#include "Settings.h"
#include "PickingVisitor.h"
#include "DrawVisitor.h"
#include "Decorations.h"
#include "UserInterfaceManager.h"
#include "Log.h"

#include "GeometryView.h"


GeometryView::GeometryView() : View(GEOMETRY)
{
    // read default settings
    if ( Settings::application.views[mode_].name.empty() ) {
        // no settings found: store application default
        Settings::application.views[mode_].name = "Geometry";
        scene.root()->scale_ = glm::vec3(GEOMETRY_DEFAULT_SCALE, GEOMETRY_DEFAULT_SCALE, 1.0f);
        saveSettings();
    }
    else
        restoreSettings();

    // Geometry Scene foreground
    output_surface_ = new Surface;
    output_surface_->visible_ = false;
    scene.fg()->attach(output_surface_);
    Frame *border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    border->color = glm::vec4( COLOR_FRAME, 1.f );
    scene.fg()->attach(border);

    // User interface foreground
    //
    // point to show POSITION
    overlay_position_ = new Symbol(Symbol::SQUARE_POINT);
    overlay_position_->scale_ = glm::vec3(0.5f, 0.5f, 1.f);
    scene.fg()->attach(overlay_position_);
    overlay_position_->visible_ = false;
    // cross to show the axis for POSITION
    overlay_position_cross_ = new Symbol(Symbol::CROSS);
    overlay_position_cross_->rotation_ = glm::vec3(0.f, 0.f, M_PI_4);
    overlay_position_cross_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_position_cross_);
    overlay_position_cross_->visible_ = false;
    // 'clock' : tic marks every 10 degrees for ROTATION
    // with dark background
    Group *g = new Group;
    Symbol *s = new Symbol(Symbol::CLOCK);
    g->attach(s);
    s = new Symbol(Symbol::CIRCLE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
    s->scale_ = glm::vec3(28.f, 28.f, 1.f);
    s->translation_.z = -0.1;
    g->attach(s);
    overlay_rotation_clock_ = g;
    overlay_rotation_clock_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_);
    overlay_rotation_clock_->visible_ = false;
    // circle to show fixed-size  ROTATION
    overlay_rotation_clock_hand_ = new Symbol(Symbol::CLOCK_H);
    overlay_rotation_clock_hand_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_clock_hand_);
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_ = new Symbol(Symbol::SQUARE);
    overlay_rotation_fix_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_fix_);
    overlay_rotation_fix_->visible_ = false;
    // circle to show the center of ROTATION
    overlay_rotation_ = new Symbol(Symbol::CIRCLE);
    overlay_rotation_->scale_ = glm::vec3(0.25f, 0.25f, 1.f);
    scene.fg()->attach(overlay_rotation_);
    overlay_rotation_->visible_ = false;
    // 'grid' : tic marks every 0.1 step for SCALING
    // with dark background
    g = new Group;
    s = new Symbol(Symbol::GRID);
    g->attach(s);
    s = new Symbol(Symbol::SQUARE_POINT);
    s->color = glm::vec4(0.f, 0.f, 0.f, 0.1f);
    s->scale_ = glm::vec3(18.f, 18.f, 1.f);
    s->translation_.z = -0.1;
    g->attach(s);
    overlay_scaling_grid_ = g;
    overlay_scaling_grid_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_grid_);
    overlay_scaling_grid_->visible_ = false;
    // cross in the square for proportional SCALING
    overlay_scaling_cross_ = new Symbol(Symbol::CROSS);
    overlay_scaling_cross_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_cross_);
    overlay_scaling_cross_->visible_ = false;
    // square to show the center of SCALING
    overlay_scaling_ = new Symbol(Symbol::SQUARE);
    overlay_scaling_->scale_ = glm::vec3(0.3f, 0.3f, 1.f);
    scene.fg()->attach(overlay_scaling_);
    overlay_scaling_->visible_ = false;

    border = new Frame(Frame::SHARP, Frame::THIN, Frame::NONE);
    border->color = glm::vec4( COLOR_HIGHLIGHT_SOURCE, 0.2f );
    overlay_crop_ = border;
    scene.fg()->attach(overlay_crop_);
    overlay_crop_->visible_ = false;

}

void GeometryView::update(float dt)
{
    View::update(dt);

    // a more complete update is requested
    if (View::need_deep_update_ > 0) {

        // update rendering of render frame
        FrameBuffer *output = Mixer::manager().session()->frame();
        if (output){
            float aspect_ratio = output->aspectRatio();
            for (NodeSet::iterator node = scene.bg()->begin(); node != scene.bg()->end(); node++) {
                (*node)->scale_.x = aspect_ratio;
            }
            for (NodeSet::iterator node = scene.fg()->begin(); node != scene.fg()->end(); node++) {
                (*node)->scale_.x = aspect_ratio;
            }
            output_surface_->setTextureIndex( output->texture() );
        }
    }

    // the current view is the geometry view
    if (Mixer::manager().view() == this )
    {
        updateSelectionOverlay();
        overlay_selection_icon_->visible_ = false;
    }
}

void GeometryView::resize ( int scale )
{
    float z = CLAMP(0.01f * (float) scale, 0.f, 1.f);
    z *= z;
    z *= GEOMETRY_MAX_SCALE - GEOMETRY_MIN_SCALE;
    z += GEOMETRY_MIN_SCALE;
    scene.root()->scale_.x = z;
    scene.root()->scale_.y = z;

    // Clamp translation to acceptable area
    glm::vec3 border(scene.root()->scale_.x * 1.5, scene.root()->scale_.y * 1.5, 0.f);
    scene.root()->translation_ = glm::clamp(scene.root()->translation_, -border, border);
}

int  GeometryView::size ()
{
    float z = (scene.root()->scale_.x - GEOMETRY_MIN_SCALE) / (GEOMETRY_MAX_SCALE - GEOMETRY_MIN_SCALE);
    return (int) ( sqrt(z) * 100.f);
}

void GeometryView::draw()
{
    // hack to prevent source manipulation (scale and rotate)
    // when multiple sources are selected: simply do not draw overlay in scene
    Source *s = Mixer::manager().currentSource();
    if (s != nullptr) {
        if ( Mixer::selection().size() > 1)   {
            s->setMode(Source::SELECTED);
            s = nullptr;
        }
    }

    // Drawing of Geometry view is different as it renders
    // only sources in the current workspace
    std::vector<Node *> surfaces;
    std::vector<Node *> overlays;
    for (auto source_iter = Mixer::manager().session()->begin();
         source_iter != Mixer::manager().session()->end(); source_iter++) {
        // if it is in the current workspace
        if ((*source_iter)->workspace() == Settings::application.current_workspace) {
            // will draw its surface
            surfaces.push_back((*source_iter)->groups_[mode_]);
            // will draw its frame and locker icon
            overlays.push_back((*source_iter)->frames_[mode_]);
            overlays.push_back((*source_iter)->locker_);
        }
    }

    // 0. prepare projection for draw visitors
    glm::mat4 projection = Rendering::manager().Projection();

    // 1. Draw surface of sources in the current workspace
    DrawVisitor draw_surfaces(surfaces, projection);
    scene.accept(draw_surfaces);

    // 2. Draw scene rendering on top (which includes rendering of all visible sources)
    DrawVisitor draw_rendering(output_surface_, projection, true);
    scene.accept(draw_rendering);

    // 3. Draw frames and icons of sources in the current workspace
    DrawVisitor draw_overlays(overlays, projection);
    scene.accept(draw_overlays);

    // 4. Draw control overlays of current source on top (if selectable)
    if (canSelect(s)) {
        s->setMode(Source::CURRENT);
        DrawVisitor dv(s->overlays_[mode_], projection);
        scene.accept(dv);
    }

    // 5. Finally, draw overlays of view
    DrawVisitor draw_foreground(scene.fg(), projection);
    scene.accept(draw_foreground);

    // display interface
    // Locate window at upper left corner
    glm::vec2 P = glm::vec2(-output_surface_->scale_.x - 0.02f, output_surface_->scale_.y + 0.01 );
    P = Rendering::manager().project(glm::vec3(P, 0.f), scene.root()->transform_, false);
    // Set window position depending on icons size
    ImGuiToolkit::PushFont(ImGuiToolkit::FONT_LARGE);
    ImGui::SetNextWindowPos(ImVec2(P.x, P.y - 1.5f * ImGui::GetFrameHeight() ), ImGuiCond_Always);
    if (ImGui::Begin("##GeometryViewOptions", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus ))
    {
        // style grey
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(COLOR_FRAME_LIGHT, 1.f));  // 1
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.14f, 0.14f, 0.14f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.46f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.85f, 0.85f, 0.85f, 0.86f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.95f, 0.95f, 0.95f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.14f, 0.14f, 0.46f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.67f, 0.67f, 0.67f, 0.79f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.36f, 0.36f, 0.36f, 0.44f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.88f, 0.88f, 0.88f, 0.73f));
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.83f, 0.83f, 0.84f, 0.78f));
        ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.53f, 0.53f, 0.53f, 0.60f));
        ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(0.40f, 0.40f, 0.40f, 1.00f));   // 14 colors

        static std::vector< std::pair<int, int> > icons_ws = { {10,16}, {11,16}, {12,16} };
        if ( ImGuiToolkit::ComboIcon (icons_ws, &Settings::application.current_workspace) ){
             View::need_deep_update_++;
        }

        ImGui::PopStyleColor(14);  // 14 colors
        ImGui::End();
    }
    ImGui::PopFont();

    // display popup menu
    if (show_context_menu_ == MENU_SOURCE) {
        ImGui::OpenPopup( "GeometrySourceContextMenu" );
        show_context_menu_ = MENU_NONE;
    }
    if (ImGui::BeginPopup("GeometrySourceContextMenu")) {
        Source *s = Mixer::manager().currentSource();
        if (s != nullptr) {
            if (ImGui::Selectable( ICON_FA_VECTOR_SQUARE "  Reset" )){
                s->group(mode_)->scale_ = glm::vec3(1.f);
                s->group(mode_)->rotation_.z = 0;
                s->group(mode_)->crop_ = glm::vec3(1.f);
                s->group(mode_)->translation_ = glm::vec3(0.f);
                s->touch();
            }
            else if (ImGui::Selectable( ICON_FA_EXPAND "  Fit" )){
                glm::vec3 scale = glm::vec3(1.f);
                FrameBuffer *output = Mixer::manager().session()->frame();
                if (output) scale.x = output->aspectRatio() / s->frame()->aspectRatio();
                s->group(mode_)->scale_ = scale;
                s->group(mode_)->rotation_.z = 0;
                s->group(mode_)->translation_ = glm::vec3(0.f);
                s->touch();
            }
            else  if (ImGui::Selectable( ICON_FA_CROSSHAIRS "  Center" )){
                s->group(mode_)->translation_ = glm::vec3(0.f);
                s->touch();
            }
            else if (ImGui::Selectable( ICON_FA_PERCENTAGE "   Original aspect ratio" )){ //ICON_FA_ARROWS_ALT_H
                s->group(mode_)->scale_.x = s->group(mode_)->scale_.y;
                s->group(mode_)->scale_.x *= s->group(mode_)->crop_.x / s->group(mode_)->crop_.y;
                s->touch();
            }
        }
        ImGui::EndPopup();
    }
}


std::pair<Node *, glm::vec2> GeometryView::pick(glm::vec2 P)
{
    // prepare empty return value
    std::pair<Node *, glm::vec2> pick = { nullptr, glm::vec2(0.f) };

    // unproject mouse coordinate into scene coordinates
    glm::vec3 scene_point_ = Rendering::manager().unProject(P);

    // picking visitor traverses the scene
    PickingVisitor pv(scene_point_);
    scene.accept(pv);

    // picking visitor found nodes?
    if ( !pv.empty() ) {
        // keep current source active if it is clicked
        Source *current = Mixer::manager().currentSource();
        if (current != nullptr) {
            if (current->workspace() != Settings::application.current_workspace){
                current = nullptr;
            }

            // find if the current source was picked
            auto itp = pv.rbegin();
            for (; itp != pv.rend(); itp++){
                // test if source contains this node
                Source::hasNode is_in_source((*itp).first );
                if ( is_in_source( current ) ){
                    // a node in the current source was clicked !
                    pick = *itp;
                    break;
                }
            }
            // not found: the current source was not clicked
            if (itp == pv.rend()) {
                current = nullptr;
            }
            // picking on the menu handle: show context menu
            else if ( pick.first == current->handles_[mode_][Handles::MENU] ) {
                openContextMenu(MENU_SOURCE);
            }
            // pick on the lock icon; unlock source
            else if ( pick.first == current->lock_ ) {
                current->setLocked(false);
            }
            // pick on the open lock icon; lock source and cancel pick
            else if ( pick.first == current->unlock_ ) {
                current->setLocked(true);
                pick = { nullptr, glm::vec2(0.f) };
            }
            // pick a locked source without CTRL key; cancel pick
            else if ( current->locked() && !UserInterface::manager().ctrlModifier() ) {
                pick = { nullptr, glm::vec2(0.f) };
            }
        }
        // the clicked source changed (not the current source)
        if (current == nullptr){

            // default to failed pick
            pick = { nullptr, glm::vec2(0.f) };

            // loop over all nodes picked
            for (auto itp = pv.rbegin(); itp != pv.rend(); itp++){

                // get if a source was picked
                Source *s = Mixer::manager().findSource((*itp).first);

                // accept picked sources in current workspaces
                if ( s!=nullptr && s->workspace() == Settings::application.current_workspace) {

                    // lock icon of a source is picked : unlock
                    if ( (*itp).first == s->lock_) {
                        s->setLocked(false);
                        pick = { s->locker_,  (*itp).second };
                        break;
                    }
                    // a non-locked source is picked (or locked with CTRL key)
                    else if ( !s->locked() || ( s->locked() && UserInterface::manager().ctrlModifier() ) ) {
                        pick = { s->locker_,  (*itp).second };
                        break;
                    }
                }

            }

        }
    }

    return pick;
}

bool GeometryView::canSelect(Source *s) {

    return ( View::canSelect(s) && s->active() && s->workspace() == Settings::application.current_workspace);
}

View::Cursor GeometryView::grab (Source *s, glm::vec2 from, glm::vec2 to, std::pair<Node *, glm::vec2> pick)
{
    View::Cursor ret = Cursor();

    // work on the given source
    if (!s)
        return ret;

    Group *sourceNode = s->group(mode_); // groups_[View::GEOMETRY]

    // grab coordinates in scene-View reference frame
    glm::vec3 scene_from = Rendering::manager().unProject(from, scene.root()->transform_);
    glm::vec3 scene_to   = Rendering::manager().unProject(to, scene.root()->transform_);
    glm::vec3 scene_translation = scene_to - scene_from;

    // make sure matrix transform of stored status is updated
    s->stored_status_->update(0);
    // grab coordinates in source-root reference frame
    glm::vec4 source_from = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_from,  1.f );
    glm::vec4 source_to   = glm::inverse(s->stored_status_->transform_) * glm::vec4( scene_to,  1.f );
    glm::vec3 source_scaling     = glm::vec3(source_to) / glm::vec3(source_from);

    // which manipulation to perform?
    std::ostringstream info;
    if (pick.first)  {
        // which corner was picked ?
        glm::vec2 corner = glm::round(pick.second);

        // transform from source center to corner
        glm::mat4 T = GlmToolkit::transform(glm::vec3(corner.x, corner.y, 0.f), glm::vec3(0.f, 0.f, 0.f),
                                            glm::vec3(1.f / s->frame()->aspectRatio(), 1.f, 1.f));

        // transformation from scene to corner:
        glm::mat4 scene_to_corner_transform = T * glm::inverse(s->stored_status_->transform_);
        glm::mat4 corner_to_scene_transform = glm::inverse(scene_to_corner_transform);

        // compute cursor movement in corner reference frame
        glm::vec4 corner_from = scene_to_corner_transform * glm::vec4( scene_from,  1.f );
        glm::vec4 corner_to   = scene_to_corner_transform * glm::vec4( scene_to,  1.f );
        // operation of scaling in corner reference frame
        glm::vec3 corner_scaling = glm::vec3(corner_to) / glm::vec3(corner_from);

        // convert source position in corner reference frame
        glm::vec4 center = scene_to_corner_transform * glm::vec4( s->stored_status_->translation_, 1.f);

        // picking on the resizing handles in the corners
        if ( pick.first == s->handles_[mode_][Handles::RESIZE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(-corner);
            // RESIZE CORNER
            // proportional SCALING with SHIFT
            if (UserInterface::manager().shiftModifier()) {
                // calculate proportional scaling factor
                float factor = glm::length( glm::vec2( corner_to ) ) / glm::length( glm::vec2( corner_from ) );
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * glm::vec3(factor, factor, 1.f);
                // discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    factor = sourceNode->scale_.x / s->stored_status_->scale_.x;
                    sourceNode->scale_.y = s->stored_status_->scale_.y * factor;
                }
                // update corner scaling to apply to center coordinates
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // non-proportional CORNER RESIZE  (normal case)
            else {
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on diagonal (corner picked)
            T = glm::rotate(glm::identity<glm::mat4>(), s->stored_status_->rotation_.z, glm::vec3(0.f, 0.f, 1.f));
            T = glm::scale(T, s->stored_status_->scale_);
            corner = T * glm::vec4( corner, 0.f, 0.f );
            ret.type = corner.x * corner.y > 0.f ? Cursor_ResizeNESW : Cursor_ResizeNWSE;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;

        }
        // picking on the BORDER RESIZING handles left or right
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_H] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(-corner);
            // SHIFT: HORIZONTAL SCALE to restore source aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                sourceNode->scale_.x = ABS(sourceNode->scale_.y) * SIGN(sourceNode->scale_.x);
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // HORIZONTAL RESIZE (normal case)
            else {
                // x scale only
                corner_scaling = glm::vec3(corner_scaling.x, 1.f, 1.f);
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // POST-CORRECTION ; discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeNS : Cursor_ResizeEW;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the BORDER RESIZING handles top or bottom
        else if ( pick.first == s->handles_[mode_][Handles::RESIZE_V] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // inform on which corner should be overlayed (opposite)
            s->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(-corner);
            // SHIFT: VERTICAL SCALE to restore source aspect ratio
            if (UserInterface::manager().shiftModifier()) {
                sourceNode->scale_.y = ABS(sourceNode->scale_.x) * SIGN(sourceNode->scale_.y);
                corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
            }
            // VERTICAL RESIZE (normal case)
            else {
                // y scale only
                corner_scaling = glm::vec3(1.f, corner_scaling.y, 1.f);
                // scale node
                sourceNode->scale_ = s->stored_status_->scale_ * corner_scaling;
                // POST-CORRECTION ; discretized scaling with ALT
                if (UserInterface::manager().altModifier()) {
                    sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                    corner_scaling = sourceNode->scale_ / s->stored_status_->scale_;
                }
            }
            // transform source center (in corner reference frame)
            center = glm::scale(glm::identity<glm::mat4>(), corner_scaling) * center;
            // convert center back into scene reference frame
            center = corner_to_scene_transform * center;
            // apply to node
            sourceNode->translation_ = glm::vec3(center);
            // show cursor depending on angle
            float c = tan(sourceNode->rotation_.z);
            ret.type = ABS(c) > 1.f ? Cursor_ResizeEW : Cursor_ResizeNS;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the CENTRER SCALING handle
        else if ( pick.first == s->handles_[mode_][Handles::SCALE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;
            // prepare overlay
            overlay_scaling_cross_->visible_ = false;
            overlay_scaling_grid_->visible_ = false;
            overlay_scaling_->visible_ = true;
            overlay_scaling_->translation_.x = s->stored_status_->translation_.x;
            overlay_scaling_->translation_.y = s->stored_status_->translation_.y;
            overlay_scaling_->rotation_.z = s->stored_status_->rotation_.z;
            overlay_scaling_->update(0);
            // PROPORTIONAL ONLY
            if (UserInterface::manager().shiftModifier()) {
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
                overlay_scaling_cross_->visible_ = true;
                overlay_scaling_cross_->copyTransform(overlay_scaling_);
            }
            // apply center scaling
            sourceNode->scale_ = s->stored_status_->scale_ * source_scaling;
            // POST-CORRECTION ; discretized scaling with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->scale_.x = ROUND(sourceNode->scale_.x, 10.f);
                sourceNode->scale_.y = ROUND(sourceNode->scale_.y, 10.f);
                overlay_scaling_grid_->visible_ = true;
                overlay_scaling_grid_->copyTransform(overlay_scaling_);
            }
            // show cursor depending on diagonal
            corner = glm::sign(sourceNode->scale_);
            ret.type = (corner.x * corner.y) > 0.f ? Cursor_ResizeNWSE : Cursor_ResizeNESW;
            info << "Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
            info << " x "  << sourceNode->scale_.y;
        }
        // picking on the CROP
        else if ( pick.first == s->handles_[mode_][Handles::CROP] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::ROTATE]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // prepare overlay
            overlay_crop_->scale_ = s->stored_status_->scale_ / s->stored_status_->crop_;
            overlay_crop_->scale_.x  *= s->frame()->aspectRatio();
            overlay_crop_->translation_.x = s->stored_status_->translation_.x;
            overlay_crop_->translation_.y = s->stored_status_->translation_.y;
            overlay_crop_->rotation_.z = s->stored_status_->rotation_.z;
            overlay_crop_->update(0);
            overlay_crop_->visible_ = true;

            // PROPORTIONAL ONLY
            if (UserInterface::manager().shiftModifier()) {
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
            }
            // calculate crop of framebuffer
            sourceNode->crop_ = s->stored_status_->crop_ * source_scaling;
            // POST-CORRECTION ; discretized crop with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->crop_.x = ROUND(sourceNode->crop_.x, 10.f);
                sourceNode->crop_.y = ROUND(sourceNode->crop_.y, 10.f);
            }
            // CLAMP crop values
            sourceNode->crop_.x = CLAMP(sourceNode->crop_.x, 0.1f, 1.f);
            sourceNode->crop_.y = CLAMP(sourceNode->crop_.y, 0.1f, 1.f);
            // apply center scaling
            s->frame()->setProjectionArea( glm::vec2(sourceNode->crop_) );
            sourceNode->scale_ = s->stored_status_->scale_ * (sourceNode->crop_ / s->stored_status_->crop_);
            // show cursor depending on diagonal
            corner = glm::sign(sourceNode->scale_);
            ret.type = (corner.x * corner.y) < 0.f ? Cursor_ResizeNWSE : Cursor_ResizeNESW;
            info << "Crop " << std::fixed << std::setprecision(3) << sourceNode->crop_.x;
            info << " x "  << sourceNode->crop_.y;
        }
        // picking on the rotating handle
        else if ( pick.first == s->handles_[mode_][Handles::ROTATE] ) {

            // hide all other grips
            s->handles_[mode_][Handles::RESIZE]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_H]->visible_ = false;
            s->handles_[mode_][Handles::RESIZE_V]->visible_ = false;
            s->handles_[mode_][Handles::SCALE]->visible_ = false;
            s->handles_[mode_][Handles::CROP]->visible_ = false;
            s->handles_[mode_][Handles::MENU]->visible_ = false;

            // ROTATION on CENTER
            overlay_rotation_->visible_ = true;
            overlay_rotation_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_->translation_.y = s->stored_status_->translation_.y;
            overlay_rotation_->update(0);
            overlay_rotation_fix_->visible_ = true;
            overlay_rotation_fix_->copyTransform(overlay_rotation_);
            overlay_rotation_clock_->visible_ = false;

            // rotation center to center of source (disregarding scale)
            glm::mat4 T = glm::translate(glm::identity<glm::mat4>(), s->stored_status_->translation_);
            source_from = glm::inverse(T) * glm::vec4( scene_from,  1.f );
            source_to   = glm::inverse(T) * glm::vec4( scene_to,  1.f );
            // compute rotation angle
            float angle = glm::orientedAngle( glm::normalize(glm::vec2(source_from)), glm::normalize(glm::vec2(source_to)));
            // apply rotation on Z axis
            sourceNode->rotation_ = s->stored_status_->rotation_ + glm::vec3(0.f, 0.f, angle);

            // POST-CORRECTION ; discretized rotation with ALT
            int degrees = int(  glm::degrees(sourceNode->rotation_.z) );
            if (UserInterface::manager().altModifier()) {
                degrees = (degrees / 10) * 10;
                sourceNode->rotation_.z = glm::radians( float(degrees) );
                overlay_rotation_clock_->visible_ = true;
                overlay_rotation_clock_->copyTransform(overlay_rotation_);
                info << "Angle " << degrees << "\u00b0"; // degree symbol
            }
            else
                info << "Angle " << std::fixed << std::setprecision(1) << glm::degrees(sourceNode->rotation_.z) << "\u00b0"; // degree symbol

            overlay_rotation_clock_hand_->visible_ = true;
            overlay_rotation_clock_hand_->translation_.x = s->stored_status_->translation_.x;
            overlay_rotation_clock_hand_->translation_.y = s->stored_status_->translation_.y;
            overlay_rotation_clock_hand_->rotation_.z = sourceNode->rotation_.z;
            overlay_rotation_clock_hand_->update(0);

            // show cursor for rotation
            ret.type = Cursor_Hand;
            // + SHIFT = no scaling /  NORMAL = with scaling
            if (!UserInterface::manager().shiftModifier()) {
                // compute scaling to match cursor
                float factor = glm::length( glm::vec2( source_to ) ) / glm::length( glm::vec2( source_from ) );
                source_scaling = glm::vec3(factor, factor, 1.f);
                // apply center scaling
                sourceNode->scale_ = s->stored_status_->scale_ * source_scaling;
                info << std::endl << "   Size " << std::fixed << std::setprecision(3) << sourceNode->scale_.x;
                info << " x "  << sourceNode->scale_.y ;
                overlay_rotation_fix_->visible_ = false;
            }
        }
        // picking anywhere but on a handle: user wants to move the source
        else {
            ret.type = Cursor_ResizeAll;
            sourceNode->translation_ = s->stored_status_->translation_ + scene_translation;
            // discretized translation with ALT
            if (UserInterface::manager().altModifier()) {
                sourceNode->translation_.x = ROUND(sourceNode->translation_.x, 10.f);
                sourceNode->translation_.y = ROUND(sourceNode->translation_.y, 10.f);
            }
            // ALT: single axis movement
            overlay_position_cross_->visible_ = false;
            if (UserInterface::manager().shiftModifier()) {
                overlay_position_cross_->visible_ = true;
                overlay_position_cross_->translation_.x = s->stored_status_->translation_.x;
                overlay_position_cross_->translation_.y = s->stored_status_->translation_.y;
                overlay_position_cross_->update(0);

                glm::vec3 dif = s->stored_status_->translation_ - sourceNode->translation_;
                if (ABS(dif.x) > ABS(dif.y) ) {
                    sourceNode->translation_.y = s->stored_status_->translation_.y;
                    ret.type = Cursor_ResizeEW;
                } else {
                    sourceNode->translation_.x = s->stored_status_->translation_.x;
                    ret.type = Cursor_ResizeNS;
                }
            }
            // Show center overlay for POSITION
            overlay_position_->visible_ = true;
            overlay_position_->translation_.x = sourceNode->translation_.x;
            overlay_position_->translation_.y = sourceNode->translation_.y;
            overlay_position_->update(0);
            // Show move cursor
            info << "Position " << std::fixed << std::setprecision(3) << sourceNode->translation_.x;
            info << ", "  << sourceNode->translation_.y ;
        }
    }

    // request update
    s->touch();

    // store action in history
    current_action_ = s->name() + ": " + info.str();
    current_id_ = s->id();

    // update cursor
    ret.info = info.str();
    return ret;
}



void GeometryView::terminate()
{
    View::terminate();

    // hide all view overlays
    overlay_position_->visible_       = false;
    overlay_position_cross_->visible_ = false;
    overlay_rotation_clock_->visible_ = false;
    overlay_rotation_clock_hand_->visible_ = false;
    overlay_rotation_fix_->visible_   = false;
    overlay_rotation_->visible_       = false;
    overlay_scaling_grid_->visible_   = false;
    overlay_scaling_cross_->visible_  = false;
    overlay_scaling_->visible_        = false;
    overlay_crop_->visible_           = false;

    // restore of all handles overlays
    glm::vec2 c(0.f, 0.f);
    for (auto sit = Mixer::manager().session()->begin();
         sit != Mixer::manager().session()->end(); sit++){

        (*sit)->handles_[mode_][Handles::RESIZE]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_H]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE_V]->overlayActiveCorner(c);
        (*sit)->handles_[mode_][Handles::RESIZE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::RESIZE_H]->visible_ = true;
        (*sit)->handles_[mode_][Handles::RESIZE_V]->visible_ = true;
        (*sit)->handles_[mode_][Handles::SCALE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::ROTATE]->visible_ = true;
        (*sit)->handles_[mode_][Handles::CROP]->visible_ = true;
        (*sit)->handles_[mode_][Handles::MENU]->visible_ = true;
    }

}

void GeometryView::arrow (glm::vec2 movement)
{
    Source *s = Mixer::manager().currentSource();
    if (s) {

        glm::vec3 gl_Position_from = Rendering::manager().unProject(glm::vec2(0.f), scene.root()->transform_);
        glm::vec3 gl_Position_to   = Rendering::manager().unProject(movement, scene.root()->transform_);
        glm::vec3 gl_delta = gl_Position_to - gl_Position_from;

        Group *sourceNode = s->group(mode_);
        if (UserInterface::manager().altModifier()) {
            sourceNode->translation_ += glm::vec3(movement.x, -movement.y, 0.f) * 0.1f;
            sourceNode->translation_.x = ROUND(sourceNode->translation_.x, 10.f);
            sourceNode->translation_.y = ROUND(sourceNode->translation_.y, 10.f);
        }
        else
            sourceNode->translation_ += gl_delta * ARROWS_MOVEMENT_FACTOR;

        // request update
        s->touch();
    }
}