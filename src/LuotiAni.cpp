#include "LuotiAni.h"

LuotiAni::Matrix2D LuotiAni::createRotationMatrix(float angle) {
    Matrix2D mat;
    float rad = angle * M_PI / 180.0f;
    float cos_angle = cosf(rad);
    float sin_angle = sinf(rad);

    mat.m[0][0] = cos_angle;
    mat.m[0][1] = -sin_angle;
    mat.m[1][0] = sin_angle;
    mat.m[1][1] = cos_angle;

    return mat;
}

SPoint LuotiAni::transformPoint(const Matrix2D *mat, SPoint point) {
    return SPoint{
        mat->m[0][0] * point.x + mat->m[0][1] * point.y,
        mat->m[1][0] * point.x + mat->m[1][1] * point.y
    };
}

uint32_t LuotiAni::getPixel(Surface *surface, int x, int y) {
    if (!surface || x < 0 || x >= surface->width() || y < 0 || y >= surface->height()) {
        return 0;
    }
    return static_cast<uint32_t*>(surface->pixels())[y * surface->width() + x];
}

void LuotiAni::setPixel(Surface *surface, int x, int y, uint32_t pixel) {
    if (!surface || x < 0 || x >= surface->width() || y < 0 || y >= surface->height()) {
        return;
    }
    static_cast<uint32_t*>(surface->pixels())[y * surface->width() + x] = pixel;
}

uint32_t LuotiAni::bilinearInterpolation(Surface *surface, float x, float y) {
    int x1 = std::floor(x);
    int y1 = std::floor(y);
    int x2 = x1 + 1;
    int y2 = y1 + 1;

    int w = surface->width();
    int h = surface->height();
    x1 = std::clamp(x1, 0, w - 1);
    y1 = std::clamp(y1, 0, h - 1);
    x2 = std::clamp(x2, 0, w - 1);
    y2 = std::clamp(y2, 0, h - 1);

    uint32_t p11 = getPixel(surface, x1, y1);
    uint32_t p12 = getPixel(surface, x1, y2);
    uint32_t p21 = getPixel(surface, x2, y1);
    uint32_t p22 = getPixel(surface, x2, y2);

    uint8_t* p11b = reinterpret_cast<uint8_t*>(&p11);
    uint8_t* p12b = reinterpret_cast<uint8_t*>(&p12);
    uint8_t* p21b = reinterpret_cast<uint8_t*>(&p21);
    uint8_t* p22b = reinterpret_cast<uint8_t*>(&p22);

    uint8_t r11 = p11b[0], g11 = p11b[1], b11 = p11b[2], a11 = p11b[3];
    uint8_t r12 = p12b[0], g12 = p12b[1], b12 = p12b[2], a12 = p12b[3];
    uint8_t r21 = p21b[0], g21 = p21b[1], b21 = p21b[2], a21 = p21b[3];
    uint8_t r22 = p22b[0], g22 = p22b[1], b22 = p22b[2], a22 = p22b[3];

    float dx = x - x1;
    float dy = y - y1;

    float r = (1 - dx) * (1 - dy) * r11 + dx * (1 - dy) * r21 + (1 - dx) * dy * r12 + dx * dy * r22;
    float g = (1 - dx) * (1 - dy) * g11 + dx * (1 - dy) * g21 + (1 - dx) * dy * g12 + dx * dy * g22;
    float b = (1 - dx) * (1 - dy) * b11 + dx * (1 - dy) * b21 + (1 - dx) * dy * b12 + dx * dy * b22;
    float a = (1 - dx) * (1 - dy) * a11 + dx * (1 - dy) * a21 + (1 - dx) * dy * a12 + dx * dy * a22;

    uint32_t result = 0;
    uint8_t* rb = reinterpret_cast<uint8_t*>(&result);
    rb[0] = static_cast<uint8_t>(r);
    rb[1] = static_cast<uint8_t>(g);
    rb[2] = static_cast<uint8_t>(b);
    rb[3] = static_cast<uint8_t>(a);
    return result;
}

SharedSurface LuotiAni::getImageFromResource(string resourceId){
    ResourceProvider* provider = getResourceProvider();
    if (provider == nullptr) {
        printf("LuotiAni::getImageFromResource: No resource provider\n");
        throw "LuotiAni::getImageFromResource: No resource provider";
    }

    shared_ptr<vector<char>> imageData = provider->readFile(resourceId);
    if (imageData == nullptr || imageData->empty()) {
        printf("LuotiAni::getImageFromResource Error: '%s' not found\n", resourceId.c_str());
        throw "LuotiAni::getImageFromResource Error: resource not found";
    }

    SharedSurface surface = Surface::loadFromMemory(imageData->data(), imageData->size());
    if (surface == nullptr) {
        printf("LuotiAni::getImageFromResource Error: loadFromMemory failed.\n");
        throw "LuotiAni::getImageFromResource Error: loadFromMemory failed.";
    }
    return surface;
}

LuotiAni::OpData LuotiAni::keyFrameToOpData(shared_ptr<KeyFrame> keyFrame, OpData srcOpData){
    if (keyFrame == nullptr) throw "LuotiAni::keyFrameToOpData: No keyframe to convert.";

    OpData opData = srcOpData;

    for (size_t o = 0; o < keyFrame->size(); o++) {
        shared_ptr<Operation> operation = (*keyFrame)[o];
        if (operation == nullptr) continue;

        switch (operation->getType()) {
            case Operation::OPERATION_TYPE::TRANSLATE:
                opData.translate = opData.translate + SPoint(operation->getP0(), operation->getP1());
                break;
            case Operation::OPERATION_TYPE::SCALE:
                opData.m = opData.m * SMultipleSize(operation->getP0(), operation->getP1());
                break;
            case Operation::OPERATION_TYPE::ROTATE:
                opData.rotate += operation->getP0();
                opData.centerPos = {operation->getP1(), operation->getP2()};
                break;
            case Operation::OPERATION_TYPE::OPACITY:
                opData.opacity = (uint8_t)(operation->getP0() * 255 / 100);
                break;
            case Operation::OPERATION_TYPE::VISIBLE:
                opData.visible = operation->getP0() > 0 ? true : false;
                break;
            default:
                break;
        }
    }
    return opData;
}


void LuotiAni::loadFromFile(fs::path filePath) {
    loadAniDesc(filePath);
}
void LuotiAni::loadFromResource(string resourceId) {
    loadAniDesc(resourceId);
}
void LuotiAni::loadAniDesc(fs::path filePath){
    FILE* stream = fopen(filePath.string().c_str(), "rb");
    if (!stream) {
        printf("Open aniDesc json file error\n");
        throw "Open aniDesc json file error";
        return;
    }
    fseek(stream, 0, SEEK_END);
    long iFileLen = ftell(stream);
    if (iFileLen <= 0) {
        fclose(stream);
        throw "Open aniDesc json file error";
        return;
    }
    fseek(stream, 0, SEEK_SET);

    m_pJsonFileContent = shared_ptr<char[]>(new char[static_cast<size_t>(iFileLen) + 1]);
    size_t bytesRead = fread(m_pJsonFileContent.get(), 1, static_cast<size_t>(iFileLen), stream);
    m_pJsonFileContent[static_cast<size_t>(iFileLen)] = '\0';
    fclose(stream);

    if (bytesRead != static_cast<size_t>(iFileLen)) {
        m_pJsonFileContent = nullptr;
        throw "Read aniDesc json file error";
        return;
    }

    parseJsonDesc();
}

void LuotiAni::loadAniDesc(string resourceId){
    ResourceProvider* provider = getResourceProvider();
    if (provider == nullptr) {
        printf("LuotiAni::loadAniDesc: No resource provider\n");
        return;
    }

    shared_ptr<vector<char>> fileData = provider->readFile(resourceId);
    if (fileData == nullptr || fileData->empty()) {
        printf("LuotiAni::loadAniDesc: '%s' not found\n", resourceId.c_str());
        return;
    }

    size_t iFileLen = fileData->size();
    m_pJsonFileContent = shared_ptr<char[]>(new char[iFileLen + 1]);
    memcpy(m_pJsonFileContent.get(), fileData->data(), iFileLen);
    m_pJsonFileContent[iFileLen] = '\0';

    parseJsonDesc();
}

void LuotiAni::parseJsonDesc(){
    m_jsonAniDesc = json::parse(m_pJsonFileContent.get(), nullptr, false, true);

    json overview = m_jsonAniDesc["overview"];
    if (overview.is_null()) {
        printf("Animation Description json error: 'overview' section missing.\n");
        throw "Animation Description json error: 'overview' section missing.";
        return;
    }
    m_name = overview["name"].get<string>();
    m_version = overview["version"].get<string>();
    m_canvasSize.width = overview["view"].at("width").get<float>();
    m_canvasSize.height = overview["view"].at("height").get<float>();
    m_frameRate = overview["frameRate"].get<int>();
    if(m_frameRate == 0) {
        printf("Animation Description json error: 'frameRate' cannot be zero.\n");
        throw "Animation Description json error: 'frameRate' cannot be zero.";
        return;
    }
    m_frameMSDuration = 1000 / m_frameRate;
    m_totalFrames = overview["totalFrames"].get<uint32_t>();
    m_loop = overview.at("loop").get<bool>();

    for (const auto& layerData : m_jsonAniDesc["layers"]) {
        auto layer = make_shared<Layer>();
        layer->setName(layerData.at("name").get<string>())
            ->setType(Layer::strToLayerType(layerData.at("type").get<string>()))
            ->setSrc(layerData.at("src").get<string>())
            ->setSize(SSize(layerData.contains("width") && layerData.contains("height") ?
                            SSize(layerData.at("width").get<float>(), layerData.at("height").get<float>()) :
                            SSize(0, 0)))
            ->setOpacity(layerData.at("opacity").get<float>() / 100.0f)
            ->setBlendMode(Layer::blendModeStrToBlendMode(layerData.at("blendMode").get<string>()));
        for (const auto& keyFrameData : layerData["keyFrames"]) {
            auto keyFrame = make_shared<KeyFrame>();
            uint32_t frameNumber = keyFrameData.at("frame").get<uint32_t>();

            auto operationsData = keyFrameData.at("operation");
            for (const auto& operationData : operationsData) {
                string type = operationData.at("type").get<string>();
                Operation::OPERATION_TYPE opType = Operation::strToOperationType(type);

                shared_ptr<Operation> operation = nullptr;
                switch(opType) {
                    case Operation::OPERATION_TYPE::TRANSLATE:
                        operation = make_shared<Operation>(opType, operationData.at("tx").get<float>(), operationData.at("ty").get<float>());
                        break;
                    case Operation::OPERATION_TYPE::SCALE:
                        operation = make_shared<Operation>(opType, operationData.at("sx").get<float>(), operationData.at("sy").get<float>());
                        break;
                    case Operation::OPERATION_TYPE::ROTATE:
                        operation = make_shared<Operation>(opType, operationData.at("angle").get<float>(), operationData.at("cx").get<float>(), operationData.at("cy").get<float>());
                        break;
                    case Operation::OPERATION_TYPE::OPACITY:
                        operation = make_shared<Operation>(opType, operationData.at("opacity").get<float>());
                        break;
                    case Operation::OPERATION_TYPE::VISIBLE:
                        operation = make_shared<Operation>(opType, operationData.at("visible").get<bool>() ? 1.0f : 0.0f);
                        break;
                    default:
                        printf("KeyFrame Operation: Unknown operation type: %s\n", type.c_str());
                        continue;
                }
                if (operation == nullptr) continue;
                keyFrame->addOperation(operation);
            }
            layer->addKeyFrame(frameNumber, keyFrame);
        }
        m_layers.push_back(layer);
    }

    m_isLoaded = true;
}


void LuotiAni::update(void) {
    if (!m_visible) return;
    if (!m_isPrepared || m_frames.empty()) return;

    if (m_isPlaying && m_totalFrames > 0) {
        uint64_t currentTick = getTicks();
        uint64_t deltaTick = currentTick - m_lastFrameMsTick;
        if (deltaTick >= m_frameMSDuration) {
            uint32_t m_nextFrameToDraw = (m_frameToDraw + deltaTick / m_frameMSDuration) % m_totalFrames;

            if (m_nextFrameToDraw < m_frameToDraw) {
                if (!m_loop) {
                    m_frameToDraw = 0;
                    m_isPlaying = false;

                    { auto evt = make_shared<Event>(EventType::Custom); evt->customInt = static_cast<int>(EventName::AnimationEnded); evt->customPtr = reinterpret_cast<void*>(static_cast<intptr_t>(m_id)); triggerEvent(evt); }
                    return;
                }
            }
            m_frameToDraw = m_nextFrameToDraw;
            m_lastFrameMsTick = currentTick;
        }
    }
}

void LuotiAni::draw(float x, float y, uint8_t alpha){
    draw(m_frameToDraw, x, y, alpha);
}

void LuotiAni::draw(uint32_t frameNo, float x, float y, uint8_t alpha) {
    if (!m_visible) return;
    if (!m_isPrepared || m_frames.empty()) return;

    m_frames[frameNo]->draw(x, y, alpha);
}

void LuotiAni::setRect(SRect rect){
    Material::setRect(rect);
    for (size_t f = 0; f < m_frames.size(); f++) {
        m_frames[f]->setRect({0, 0, rect.width, rect.height});
    }
}

void LuotiAni::play(void){
    if (!m_isPrepared) {
        printf("LuotiAni::play: Animation not prepared.\n");
        throw "LuotiAni::play: Animation not prepared.";
        return;
    }
    m_frameToDraw = 0;
    m_isPlaying = true;
    m_lastFrameMsTick = getTicks();
}

void LuotiAni::setRenderDevice(RenderDevice* device) {
    Material::setRenderDevice(device);

    if (device != nullptr) {
        for (size_t i = 0; i < m_frames.size() && i < m_frameSurfaces.size(); i++) {
            Actor* frameActor = dynamic_cast<Actor*>(m_frames[i].get());
            if (frameActor != nullptr && frameActor->getTexture() == nullptr && m_frameSurfaces[i] != nullptr) {
                auto tex = m_frameSurfaces[i]->createTexture(device);
                if (tex != nullptr) {
                    frameActor->setTexture(tex);
                }
            }
        }
    }
}

void LuotiAni::prepare(uint32_t startFrame){
    if (!m_isLoaded) {
        printf("LuotiAni::prepare: Animation description not loaded.\n");
        throw "LuotiAni::prepare: Animation description not loaded.";
        return;
    }
    if (m_isPrepared) {
        printf("LuotiAni::prepare: Animation already prepared.\n");
        return;
    }

    m_frameToDraw = startFrame;

    if (m_rect.width <= 0 || m_rect.height <= 0) {
        m_rect.width = m_canvasSize.width;
        m_rect.height = m_canvasSize.height;
    }
    vector<OpData> frameOp;
    vector<vector<OpData>> allFrameOp;
    for (uint32_t l = 0; l < m_layers.size(); l++) {
        shared_ptr<Layer> layer = m_layers[l];
        if (layer == nullptr) continue;
        SharedSurface operationSurface;
        switch( layer->getType() ){
            case Layer::LAYER_TYPE::IMAGE:
                operationSurface = getImageFromResource(layer->getSrc());

                if(layer->getSize().width == 0) {
                    layer->setSize({(float)operationSurface->width(), layer->getSize().height});
                }
                if(layer->getSize().height == 0) {
                    layer->setSize({layer->getSize().width, (float)operationSurface->height()});
                }
                if(layer->getSize().width != operationSurface->width() || layer->getSize().height != operationSurface->height()){
                    SharedSurface scaledImageSurface = Surface::create(layer->getSize().width, layer->getSize().height);
                    if (scaledImageSurface == nullptr) {
                        printf("LuotiAni::prepare: Create scaled image surface error\n");
                        throw "LuotiAni::prepare: Create scaled image surface error";
                    }
                    scaledImageSurface->blit(operationSurface.get(), 0, 0, operationSurface->width(), operationSurface->height(),
                                              0, 0, scaledImageSurface->width(), scaledImageSurface->height());
                    operationSurface = scaledImageSurface;
                }
                break;
            case Layer::LAYER_TYPE::SHAPE:
                continue;
            case Layer::LAYER_TYPE::TEXT:
                continue;
            default:
                continue;
        }
        if (operationSurface == nullptr) {
            printf("LuotiAni::prepare: No surface for layer %d.\n", l);
            throw "LuotiAni::prepare: No surface for layer.";
            return;
        }

        OpData opData;
        opData.opacity = layer->getOpacity();
        opData.dRect = {0, 0, (float)operationSurface->width(), (float)operationSurface->height()};
        opData.m = {1, 1};
        opData.surface = operationSurface;

        shared_ptr<KeyFrame> keyFrame = (*layer)[0];
        if (keyFrame == nullptr) throw "LuotiAni::prepare: No keyframe for frame 0.";
        opData = keyFrameToOpData(keyFrame, opData);

        frameOp.push_back(opData);
        allFrameOp.push_back(frameOp);
        frameOp.clear();
    }

    for (uint32_t l = 0; l < m_layers.size(); l++) {
        shared_ptr<Layer> layer = m_layers[l];
        if (layer == nullptr) continue;

        frameOp = allFrameOp[l];

        uint32_t previousFrameNumber = 0;
        OpData previousOpData = frameOp[previousFrameNumber];

        uint32_t previousKeyFrame = 0;
        uint32_t nextKeyFrame = layer->nextKeyFrameNumber(previousKeyFrame);
        while (nextKeyFrame != 0) {
            shared_ptr<KeyFrame> keyFrame = (*layer)[nextKeyFrame];
            if (keyFrame == nullptr) break;
            OpData opData = keyFrameToOpData(keyFrame, previousOpData);

            for (uint32_t f = 1; f < nextKeyFrame - previousKeyFrame; f++){
                OpData autoOpData;
                float t = (float)f / (nextKeyFrame - previousKeyFrame);

                autoOpData.dRect        = previousOpData.dRect;
                autoOpData.translate.x  = previousOpData.translate.x    + (opData.translate.x   - previousOpData.translate.x)   * t;
                autoOpData.translate.y  = previousOpData.translate.y    + (opData.translate.y   - previousOpData.translate.y)   * t;
                autoOpData.m.scaleX     = previousOpData.m.scaleX       + (opData.m.scaleX      - previousOpData.m.scaleX)      * t;
                autoOpData.m.scaleY     = previousOpData.m.scaleY       + (opData.m.scaleY      - previousOpData.m.scaleY)      * t;
                autoOpData.rotate       = previousOpData.rotate         + (opData.rotate        - previousOpData.rotate)        * t;
                autoOpData.centerPos.x  = previousOpData.centerPos.x    + (opData.centerPos.x   - previousOpData.centerPos.x)   * t;
                autoOpData.centerPos.y  = previousOpData.centerPos.y    + (opData.centerPos.y   - previousOpData.centerPos.y)   * t;
                autoOpData.opacity      = previousOpData.opacity        + (opData.opacity       - previousOpData.opacity)       * t;
                autoOpData.visible      = previousOpData.visible;
                autoOpData.surface      = previousOpData.surface;

                frameOp.push_back(autoOpData);
            }
            frameOp.push_back(opData);

            previousKeyFrame = nextKeyFrame;
            previousOpData = opData;
            nextKeyFrame = layer->nextKeyFrameNumber(previousKeyFrame);

        }
        while (previousKeyFrame + 1 < m_totalFrames) {
            frameOp.push_back(previousOpData);
            previousKeyFrame++;
        }
        allFrameOp[l] = frameOp;
    }

    for (uint32_t f = 0; f < m_totalFrames; f++) {
        SharedSurface canvas = Surface::create(m_canvasSize.width, m_canvasSize.height);
        if (canvas == nullptr) {
            printf("LuotiAni::prepare: Create canvas surface error\n");
            throw "LuotiAni::prepare: Create canvas surface error";
            return;
        }

        for(uint32_t l = 0; l < m_layers.size(); l++) {
            shared_ptr<Layer> layer = m_layers[l];
            if (layer == nullptr) continue;

            OpData opData = allFrameOp[l][f];

            if(!opData.visible) {
                continue;
            }

            if (opData.surface == nullptr) continue;
            Surface* srcSurface = opData.surface.get();

            SharedSurface rotatedHolder;
            if(opData.rotate != 0) {
                rotatedHolder = srcSurface->rotate(opData.rotate, getRenderDevice());
                if (rotatedHolder == nullptr) {
                    printf("LuotiAni::prepare: rotate surface error.\n");
                    throw "LuotiAni::prepare: rotate surface error.";
                }
                srcSurface = rotatedHolder.get();
            }

            SRect dRect = (opData.dRect + opData.translate) * opData.m;

            srcSurface->setAlphaMod(opData.opacity);
            srcSurface->setBlendMode(layer->getBlendMode());

            canvas->blit(srcSurface, 0, 0, srcSurface->width(), srcSurface->height(),
                        (int)dRect.left, (int)dRect.top, (int)dRect.width, (int)dRect.height);
        }
        shared_ptr<Actor> frame = make_shared<Actor>(this, true);
        frame->setRect({0, 0, m_rect.width, m_rect.height});
        auto tex = canvas->createTexture(getRenderDevice());
        frame->setTexture(tex);
        m_frames.push_back(frame);

        m_frameSurfaces.push_back(canvas);
    }
    for (uint32_t l = 0; l < m_layers.size(); l++) {
        OpData opData = allFrameOp[l][0];
        opData.surface = nullptr;
    }

    m_isPrepared = true;
}
void LuotiAni::pause(void){
    m_isPlaying = false;
}
void LuotiAni::resume(void) {
    if (!m_isPrepared) {
        printf("LuotiAni::resume: Animation not prepared.\n");
        throw "LuotiAni::resume: Animation not prepared.";
        return;
    }
    m_isPlaying = true;
    m_lastFrameMsTick = getTicks();
}

LuotiAniBuilder& LuotiAniBuilder::loadAniDesc(fs::path filePath){
    m_luoAni->loadAniDesc(filePath);
    return *this;
}
LuotiAniBuilder& LuotiAniBuilder::loadAniDesc(string resourceId){
    m_luoAni->loadAniDesc(resourceId);
    return *this;
}
LuotiAniBuilder& LuotiAniBuilder::setRect(SRect rect){
    m_luoAni->setRect(rect);
    return *this;
}
LuotiAniBuilder& LuotiAniBuilder::prepare(uint32_t startFrame){
    m_luoAni->prepare(startFrame);
    return *this;
}
LuotiAniBuilder& LuotiAniBuilder::setLoop(bool loop){
    m_luoAni->setLoop(loop);
    return *this;
}
LuotiAniBuilder& LuotiAniBuilder::setAutoStart(){
    m_luoAni->play();
    return *this;
}

void LuotiInstance::update(void) {
    if (!m_visible) return;
    if (!m_luoAni->isPrepared()) return;

    if (m_isPlaying) {
        uint64_t currentTick = getTicks();
        uint64_t deltaTick = currentTick - m_lastFrameMsTick;
        if (deltaTick >= m_luoAni->getFrameDuration()) {
            uint32_t m_nextFrameToDraw = (m_frameToDraw + deltaTick / m_luoAni->getFrameDuration()) % m_luoAni->getTotalFrames();

            if (m_nextFrameToDraw < m_frameToDraw) {
                if (!m_luoAni->isLoop()) {
                    m_frameToDraw = 0;
                    m_isPlaying = false;

                    { auto evt = make_shared<Event>(EventType::Custom); evt->customInt = static_cast<int>(EventName::LuotiInstanceEnded); evt->customPtr = reinterpret_cast<void*>(static_cast<intptr_t>(m_userId)); triggerEvent(evt); }
                    return;
                }
            }
            m_frameToDraw = m_nextFrameToDraw;
            m_lastFrameMsTick = currentTick;
        }
    }
}

void LuotiInstance::draw(float x, float y, uint8_t alpha){
    if (!m_visible) return;
    m_luoAni->draw(m_frameToDraw, x, y, alpha);
}

void LuotiInstance::play(void){
    if (!m_luoAni->isPrepared()) {
        printf("LuotiAni::play: Animation not prepared.\n");
        throw "LuotiAni::play: Animation not prepared.";
        return;
    }
    m_frameToDraw = 0;
    m_isPlaying = true;
    m_lastFrameMsTick = getTicks();
}
