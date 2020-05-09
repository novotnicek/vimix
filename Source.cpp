
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

#include "Source.h"

#include "defines.h"
#include "FrameBuffer.h"
#include "ImageShader.h"
#include "ImageProcessingShader.h"
#include "Resource.h"
#include "Primitives.h"
#include "Mesh.h"
#include "MediaPlayer.h"
#include "SearchVisitor.h"


Source::Source(const std::string &name) : name_(name), initialized_(false)
{
    sprintf(initials_, "__");

    // create groups for each view
    // default rendering node
    groups_[View::RENDERING] = new Group;

    // default mixing nodes
    groups_[View::MIXING] = new Group;
    Frame *frame = new Frame(Frame::ROUND_THIN);
    frame->translation_.z = 0.1;
    frame->color = glm::vec4( 0.8f, 0.8f, 0.0f, 0.9f);
    groups_[View::MIXING]->attach(frame);
    groups_[View::MIXING]->scale_ = glm::vec3(0.15f, 0.15f, 1.f);

    // default geometry nodes
    groups_[View::GEOMETRY] = new Group;

    // will be associated to nodes later
    blendingshader_ = new ImageShader;
    rendershader_ = new ImageProcessingShader;
    renderbuffer_ = nullptr;
    rendersurface_ = nullptr;
    overlay_ = nullptr;
}

Source::~Source()
{
    // delete render objects
    if (renderbuffer_)
        delete renderbuffer_;

    // all groups and their children are deleted in the scene
    // this includes rendersurface_ , overlay_, blendingshader_ and rendershader_
    delete groups_[View::RENDERING];
    delete groups_[View::MIXING];
    delete groups_[View::GEOMETRY];

    groups_.clear();

}

void Source::setName (const std::string &name)
{
    name_ = name;

    initials_[0] = std::toupper( name_.front() );
    initials_[1] = std::toupper( name_.back() );
}

void Source::accept(Visitor& v)
{
    v.visit(*this);
}

void Source::setOverlayVisible(bool on)
{
    if (overlay_)
        overlay_->visible_ = on;
}

bool hasNode::operator()(const Source* elem) const
{
    if (elem)
    {
        SearchVisitor sv(_n);
        elem->group(View::MIXING)->accept(sv);
        if (sv.found())
            return true;
        elem->group(View::GEOMETRY)->accept(sv);
        if (sv.found())
            return true;
        elem->group(View::RENDERING)->accept(sv);
        return sv.found();
    }

    return false;
}

MediaSource::MediaSource(const std::string &name) : Source(name), uri_("")
{
    // create media player
    mediaplayer_ = new MediaPlayer;

    // create media surface:
    // - textured with original texture from media player
    // - crop & repeat UV can be managed here
    // - additional custom shader can be associated
    mediasurface_ = new Surface(rendershader_);

    // extra overlay for mixing view
    overlay_ = new Frame(Frame::ROUND_LARGE);
    overlay_->overlay_ = new Mesh("mesh/icon_video.ply");
    overlay_->translation_.z = 0.1;
    overlay_->color = glm::vec4( 0.8f, 0.8f, 0.0f, 1.f);
    overlay_->visible_ = false;
    groups_[View::MIXING]->attach(overlay_);

}

MediaSource::~MediaSource()
{
    // TODO delete media surface with visitor
    delete mediasurface_;

    delete mediaplayer_;
    // TODO verify that all surfaces and node is deleted in Source destructor
}

void MediaSource::setURI(const std::string &uri)
{
    uri_ = uri;
    mediaplayer_->open(uri_);
    mediaplayer_->play(true);
}

std::string MediaSource::uri() const
{
    return uri_;
}

MediaPlayer *MediaSource::mediaplayer() const
{
    return mediaplayer_;
}

void MediaSource::init()
{
    if ( mediaplayer_->isOpen() ) {

        // update video
        mediaplayer_->update();

        // once the texture of media player is created
        if (mediaplayer_->texture() != Resource::getTextureBlack()) {

            // get the texture index from media player, apply it to the media surface
            mediasurface_->setTextureIndex( mediaplayer_->texture() );

            // create Frame buffer matching size of media player
            renderbuffer_ = new FrameBuffer(mediaplayer()->width(), mediaplayer()->height());

//            // setup shader resolution for texture 0
//            rendershader_->iChannelResolution[0] = glm::vec3(mediaplayer()->width(), mediaplayer()->height(), 0.f);

            // create the surfaces to draw the frame buffer in the views
            // TODO Provide the source custom effect shader
            rendersurface_ = new FrameBufferSurface(renderbuffer_, blendingshader_);
            groups_[View::RENDERING]->attach(rendersurface_);
            groups_[View::GEOMETRY]->attach(rendersurface_);
            groups_[View::MIXING]->attach(rendersurface_);

            // for mixing view, add another surface to overlay (for stippled view in transparency)
            Surface *surfacemix = new FrameBufferSurface(renderbuffer_);
            ImageShader *is = static_cast<ImageShader *>(surfacemix->shader());
            if (is)  is->stipple = 1.0;
            groups_[View::MIXING]->attach(surfacemix);

            // scale all mixing nodes to match aspect ratio of the media
            for (NodeSet::iterator node = groups_[View::MIXING]->begin();
                 node != groups_[View::MIXING]->end(); node++) {
                (*node)->scale_.x = mediaplayer_->aspectRatio();
            }

//            mixingshader_->mask = ImageShader::mask_presets["Halo"];

            // done init once and for all
            initialized_ = true;
        }
    }

}

void MediaSource::render()
{
    if (!initialized_)
        init();
    else {
        // update video
        mediaplayer_->update();

        // render the media player into frame buffer
        static glm::mat4 projection = glm::ortho(-1.f, 1.f, 1.f, -1.f, -1.f, 1.f);
        renderbuffer_->begin();
        mediasurface_->draw(glm::identity<glm::mat4>(), projection);
        renderbuffer_->end();

        // read position of the mixing node and interpret this as transparency of render output
        float alpha = 1.0 - CLAMP( SQUARE( glm::length(groups_[View::MIXING]->translation_) ), 0.f, 1.f );
        blendingshader_->color.a = alpha;

        // TODO modify geometry
        groups_[View::RENDERING]->translation_ = groups_[View::GEOMETRY]->translation_;

    }
}

FrameBuffer *MediaSource::frame() const
{
    if (initialized_ && renderbuffer_)
    {
        return renderbuffer_;
    }
    else {
        static FrameBuffer *black = new FrameBuffer(640,480);
        return black;
    }

}

void MediaSource::accept(Visitor& v)
{
    Source::accept(v);
    v.visit(*this);
}