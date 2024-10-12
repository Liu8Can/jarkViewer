#include "Utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stbText.h"
#include "ImageDatabase.h"

#include "D2D1App.h"
#include <wrl.h>

/* TODO
1. svg: lunasvg库不支持某些特性，部分svg无法解码 考虑 https://github.com/GNOME/librsvg
2. eps
3. 在鼠标光标位置缩放
*/

const wstring appName = L"JarkViewer v1.17";



struct CurImageParameter {
    static const vector<int64_t> ZOOM_LIST;
    static const int64_t ZOOM_BASE = (1 << 16); // 100%缩放

    int64_t zoomTarget;     // 设定的缩放比例
    int64_t zoomCur;        // 动画播放过程的缩放比例，动画完毕后的值等于zoomTarget
    int curFrameIdx;        // 小于0则单张静态图像，否则为动画当前帧索引
    int curFrameIdxMax;     // 若是动画则为帧数量
    int curFrameDelay;      // 当前帧延迟
    Cood slideCur, slideTarget;
    Frames* framesPtr = nullptr;

    vector<int64_t> zoomList;
    int zoomIndex=0;
    bool isAnimation = false;
    int width = 0;
    int height = 0;
    int rotation = 0; // 旋转： 0正常， 1顺90度， 2：180度， 3顺270度/逆90度

    CurImageParameter() {
        Init();
    }

    void Init(int winWidth = 0, int winHeight = 0) {

        curFrameIdx = 0;
        curFrameDelay = 0;

        slideCur = 0;
        slideTarget = 0;
        rotation = 0;

        if (framesPtr) {
            isAnimation = framesPtr->imgList.size() > 1;
            curFrameIdxMax = (int)framesPtr->imgList.size() - 1;
            width = framesPtr->imgList[0].img.cols;
            height = framesPtr->imgList[0].img.rows;

            //适应显示窗口宽高的缩放比例
            int64_t zoomFitWindow = std::min(winWidth * ZOOM_BASE / width, winHeight * ZOOM_BASE / height);
            zoomTarget = (height > winHeight || width > winWidth) ? zoomFitWindow : ZOOM_BASE;
            zoomCur = zoomTarget;

            zoomList = ZOOM_LIST;
            if (!Utils::is_power_of_two(zoomFitWindow) || zoomFitWindow < ZOOM_LIST.front() || zoomFitWindow > ZOOM_LIST.back())
                zoomList.push_back(zoomFitWindow);
            std::sort(zoomList.begin(), zoomList.end());
            auto it = std::find(zoomList.begin(), zoomList.end(), zoomTarget);
            zoomIndex = (it != zoomList.end()) ? (int)std::distance(zoomList.begin(), it) : (int)(ZOOM_LIST.size() / 2);
        }
        else {
            isAnimation = false;
            curFrameIdxMax = 0;
            width = 0;
            height = 0;

            zoomList = ZOOM_LIST;
            zoomIndex = (int)(ZOOM_LIST.size() / 2);
            zoomTarget = ZOOM_BASE;
            zoomCur = ZOOM_BASE;
        }
    }

    void handleNewSize(int winWidth = 0, int winHeight = 0) {
        if (winHeight == 0 || winWidth == 0 || framesPtr == nullptr)
            return;

        //适应显示窗口宽高的缩放比例
        int64_t zoomFitWindow = std::min(winWidth * ZOOM_BASE / width, winHeight * ZOOM_BASE / height);

        zoomList = ZOOM_LIST;
        if (!Utils::is_power_of_two(zoomFitWindow) || zoomFitWindow < ZOOM_LIST.front() || zoomFitWindow > ZOOM_LIST.back())
            zoomList.push_back(zoomFitWindow);
        else {
            if (zoomIndex >= zoomList.size())
                zoomIndex = (int)zoomList.size() - 1;
        }
        std::sort(zoomList.begin(), zoomList.end());
    }
};

const vector<int64_t> CurImageParameter::ZOOM_LIST = {
    1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, 1 << 15, 1 << 16,
    1 << 17, 1 << 18, 1 << 19, 1 << 20, 1 << 21, 1 << 22, 
};

class OperateQueue {
private:
    std::queue<Action> queue;
    std::mutex mtx;

public:
    void push(Action action) {
        std::unique_lock<std::mutex> lock(mtx);
        queue.push(action);
    }

    Action get() {
        std::unique_lock<std::mutex> lock(mtx);

        if (queue.empty())
            return { ActionENUM::none };

        Action res = queue.front();
        queue.pop();
        return res;
    }
};

class JarkViewerApp : public D2D1App {
public:
    const int BG_GRID_WIDTH = 16;
    const uint32_t BG_COLOR = 0x46;
    const uint32_t GRID_DARK = 0xFF282828;
    const uint32_t GRID_LIGHT = 0xFF3C3C3C;

    OperateQueue operateQueue;

    CursorPos cursorPos = CursorPos::centerArea;
    CursorPos cursorPosLast = CursorPos::centerArea;
    ShowEdgeArrow showEdgeArrow = ShowEdgeArrow::none;
    bool mouseIsPressing = false;
    bool showExif = false;
    Cood mousePos, mousePressPos;
    int winWidth = 800;
    int winHeight = 600;
    bool hasInitWinSize = false;

    ImageDatabase imgDB;

    int curFileIdx = -1;         // 文件在路径列表的索引
    vector<wstring> imgFileList; // 工作目录下所有图像文件路径

    stbText stb;                 // 给Mat绘制文字
    cv::Mat mainCanvas;          // 窗口内容画布

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> pBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)
    );

    CurImageParameter curPar;

    JarkViewerApp() {
        m_wndCaption = appName;
    }

    ~JarkViewerApp() {
    }

    HRESULT Initialize(HINSTANCE hInstance, int nCmdShow, LPWSTR lpCmdLine) {
        namespace fs = std::filesystem;

        if (!SUCCEEDED(D2D1App::Initialize(hInstance, nCmdShow))) {
            return S_FALSE;
        }
        if (hInstance) Utils::setCvWindowIcon(hInstance, m_hWnd, IDI_JARKVIEWER);


        wstring filePath(*lpCmdLine == '\0' ? L"" : (*lpCmdLine == '\"' ? lpCmdLine + 1 : lpCmdLine));
        if (filePath.length() && filePath[filePath.length() - 1] == L'\"')
            filePath.pop_back();

        fs::path fullPath = fs::absolute(filePath);
        wstring fileName = fullPath.filename().wstring();

        auto workDir = fullPath.parent_path();
        if (fs::exists(workDir)) {
            for (const auto& entry : fs::directory_iterator(workDir)) {
                if (!entry.is_regular_file())continue;

                std::wstring ext = entry.path().extension().wstring();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

                if (ImageDatabase::supportExt.contains(ext) || ImageDatabase::supportRaw.contains(ext)) {
                    imgFileList.push_back(fs::absolute(entry.path()).wstring());
                    if (curFileIdx == -1 && fileName == entry.path().filename())
                        curFileIdx = (int)imgFileList.size() - 1;
                }
            }
        }
        else {
            curFileIdx = -1;
        }

        if (curFileIdx == -1) {
            if (filePath.empty()) { //直接打开软件，没有传入参数
                imgFileList.emplace_back(appName);
                curFileIdx = 0;
                imgDB.put(appName, { {{ImageDatabase::getHomeMat(), 0}}, "请在图像文件右键使用本软件打开" });
            }
            else { // 打开的文件不支持，默认加到尾部
                imgFileList.emplace_back(fullPath.wstring());
                curFileIdx = (int)imgFileList.size() - 1;
                imgDB.put(fullPath.wstring(), { {{ImageDatabase::getDefaultMat(), 0}}, "图像格式不支持或已删除" });
            }
        }

        curPar.framesPtr = imgDB.getPtr(imgFileList[curFileIdx]);

        if (m_pD2DDeviceContext == nullptr) {
            MessageBoxW(NULL, L"窗口创建失败！", L"错误", MB_ICONERROR);
            exit(-1);
        }

        return S_OK;
    }

    void OnMouseDown(WPARAM btnState, int x, int y) {
        switch ((long long)btnState)
        {
        case WM_LBUTTONDOWN: {//左键
            mouseIsPressing = true;
            mousePressPos = { x, y };

            if(cursorPos == CursorPos::leftEdge)
                operateQueue.push({ ActionENUM::preImg });
            else if (cursorPos == CursorPos::rightEdge)
                operateQueue.push({ ActionENUM::nextImg });
            return;
        }

        case WM_RBUTTONDOWN: {//右键
            return;
        }

        case WM_MBUTTONDOWN: {//中键
            return;
        }

        default:
            return;
        }
    }

    void OnMouseUp(WPARAM btnState, int x, int y) {
        using namespace std::chrono;

        switch ((long long)btnState)
        {
        case WM_LBUTTONUP: {//左键
            mouseIsPressing = false;

            if (cursorPos == CursorPos::centerArea) {
                static auto lastClickTimestamp = system_clock::now();

                auto now = system_clock::now();
                auto elapsed = duration_cast<milliseconds>(now - lastClickTimestamp).count();
                lastClickTimestamp = now;

                if (10 < elapsed && elapsed < 300) { // 10 ~ 300 ms
                    Utils::ToggleFullScreen(m_hWnd);
                }
            }
            return;
        }

        case WM_RBUTTONUP: {//右键
            operateQueue.push({ ActionENUM::requitExit });
            return;
        }

        case WM_MBUTTONUP: {//中键
            operateQueue.push({ ActionENUM::toggleExif });
            return;
        }

        default:
            return;
        }
    }

    void OnMouseMove(WPARAM btnState, int x, int y) {
        mousePos = { x, y };

        if (winWidth >= 500) {
            cursorPos = (0 <= x && x < 50) ? (CursorPos::leftEdge) : (((winWidth - 50) < x && x <= winWidth) ? CursorPos::rightEdge : CursorPos::centerArea);
        }
        else {
            cursorPos = (0 <= x && x < (winWidth / 5)) ? (CursorPos::leftEdge) : (((winWidth * 4 / 5) < x && x <= winWidth) ? CursorPos::rightEdge : CursorPos::centerArea);
        }

        if (cursorPosLast != cursorPos) {
            if (cursorPos == CursorPos::centerArea) {
                showEdgeArrow = ShowEdgeArrow::none;
                operateQueue.push({ ActionENUM::normalFresh });
            }
            else if (cursorPos == CursorPos::leftEdge) {
                showEdgeArrow = ShowEdgeArrow::left;
                operateQueue.push({ ActionENUM::normalFresh });
            }
            else if (cursorPos == CursorPos::rightEdge) {
                showEdgeArrow = ShowEdgeArrow::right;
                operateQueue.push({ ActionENUM::normalFresh });
            }

            cursorPosLast = cursorPos;
        }

        if (mouseIsPressing) {
            auto slideDelta = mousePos - mousePressPos;
            mousePressPos = mousePos;
            operateQueue.push({ ActionENUM::slide, slideDelta.x, slideDelta.y });
        }
    }

    void OnMouseLeave() {
        cursorPosLast = cursorPos = CursorPos::centerArea;
        showEdgeArrow = ShowEdgeArrow::none;
        operateQueue.push({ ActionENUM::normalFresh });
    }

    void OnMouseWheel(UINT nFlags, short zDelta, int x, int y) {
        operateQueue.push({
            (cursorPos == CursorPos::centerArea) ?
            (zDelta < 0 ? ActionENUM::zoomIn : ActionENUM::zoomOut) :
            (zDelta < 0 ? ActionENUM::nextImg : ActionENUM::preImg)
            });
    }

    void OnKeyDown(WPARAM keyValue) {
        switch (keyValue)
        {
        case VK_SPACE: { // 按空格复制图像信息到剪贴板
            Utils::copyToClipboard(Utils::utf8ToWstring(imgDB.get(imgFileList[curFileIdx]).exifStr));
        }break;

        case VK_F11: {
            Utils::ToggleFullScreen(m_hWnd);
        }break;

        case 'W':
        case VK_UP: {
            operateQueue.push({ ActionENUM::zoomOut });
        }break;

        case 'S':
        case VK_DOWN: {
            operateQueue.push({ ActionENUM::zoomIn });
        }break;

        case 'A':
        case VK_PRIOR:
        case VK_LEFT: {
            operateQueue.push({ ActionENUM::preImg });
        }break;

        case 'D':
        case VK_NEXT:
        case VK_RIGHT: {
            operateQueue.push({ ActionENUM::nextImg });
        }break;

        case 'I': {
            operateQueue.push({ ActionENUM::toggleExif });
        }break;

        case VK_ESCAPE: { // ESC
            operateQueue.push({ ActionENUM::requitExit });
        }break;

        default: {
            cout << "KeyValue: " << (int)keyValue << '\n';
        }break;
        }
    }

    void OnResize(UINT width, UINT height) {
        operateQueue.push({ ActionENUM::newSize, (int)width, (int)height });
    }

    uint32_t getSrcPx3(const cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) const {
        cv::Vec3b srcPx = srcImg.at<cv::Vec3b>(srcY, srcX);

        intUnion ret = 255;
        if (curPar.zoomCur < curPar.ZOOM_BASE && srcY > 0 && srcX > 0) { // 简单临近像素平均
            cv::Vec3b px0 = srcImg.at<cv::Vec3b>(srcY - 1, srcX - 1);
            cv::Vec3b px1 = srcImg.at<cv::Vec3b>(srcY - 1, srcX);
            cv::Vec3b px2 = srcImg.at<cv::Vec3b>(srcY, srcX - 1);
            for (int i = 0; i < 3; i++)
                ret[i] = (px0[i] + px1[i] + px2[i] + srcPx[i]) / 4;

            return ret.u32;
        }
        ret[0] = srcPx[0];
        ret[1] = srcPx[1];
        ret[2] = srcPx[2];
        return ret.u32;
    }

    uint32_t getSrcPx4(const cv::Mat& srcImg, int srcX, int srcY, int mainX, int mainY) const {

        auto srcPtr = (intUnion*)srcImg.ptr();
        int srcW = srcImg.cols;

        intUnion srcPx = srcPtr[srcW * srcY + srcX];
        intUnion bgPx = ((mainX / BG_GRID_WIDTH + mainY / BG_GRID_WIDTH) & 1) ?
            GRID_DARK : GRID_LIGHT;

        if (srcPx[3] == 0) return bgPx.u32;

        intUnion px;
        if (curPar.zoomCur < curPar.ZOOM_BASE && srcY > 0 && srcX > 0) { // 简单临近像素平均
            intUnion srcPx1 = srcPtr[srcW * (srcY - 1) + srcX - 1];
            intUnion srcPx2 = srcPtr[srcW * (srcY - 1) + srcX];
            intUnion srcPx3 = srcPtr[srcW * srcY + srcX - 1];
            for (int i = 0; i < 4; i++)
                px[i] = (srcPx1[i] + srcPx2[i] + srcPx3[i] + srcPx[i]) / 4;
        }
        else {
            px = srcPx;
        }

        if (px[3] == 255) return px.u32;

        const int alpha = px[3];
        intUnion ret = alpha;
        for (int i = 0; i < 3; i++)
            ret[i] = (bgPx[i] * (255 - alpha) + px[i] * alpha) / 256;
        return ret.u32;
    }

    void drawCanvas(const cv::Mat& srcImg, cv::Mat& canvas) const {

        const int srcH = srcImg.rows;
        const int srcW = srcImg.cols;

        const int canvasH = canvas.rows;
        const int canvasW = canvas.cols;

        if (srcH <= 0 || srcW <= 0)
            return;

        const int deltaW = curPar.slideCur.x + (int)((canvasW - srcW * curPar.zoomCur / curPar.ZOOM_BASE) / 2);
        const int deltaH = curPar.slideCur.y + (int)((canvasH - srcH * curPar.zoomCur / curPar.ZOOM_BASE) / 2);

        int xStart = deltaW < 0 ? 0 : deltaW;
        int yStart = deltaH < 0 ? 0 : deltaH;
        int xEnd = (int)(srcW * curPar.zoomCur / curPar.ZOOM_BASE + deltaW);
        int yEnd = (int)(srcH * curPar.zoomCur / curPar.ZOOM_BASE + deltaH);
        if (xEnd > canvasW) xEnd = canvasW;
        if (yEnd > canvasH) yEnd = canvasH;

        memset(canvas.ptr(), BG_COLOR, 4ULL * canvasH * canvasW);

        auto ptr = (uint32_t*)canvas.ptr();

        if (srcImg.channels() == 3) {
//#pragma omp parallel for // CPU使用率太高
            for (int y = yStart; y < yEnd; y++)
                for (int x = xStart; x < xEnd; x++) {
                    const int srcX = (int)(((int64_t)x - deltaW) * curPar.ZOOM_BASE / curPar.zoomCur);
                    const int srcY = (int)(((int64_t)y - deltaH) * curPar.ZOOM_BASE / curPar.zoomCur);
                    if (0 <= srcX && srcX < srcW && 0 <= srcY && srcY < srcH)
                        ptr[y * canvasW + x] = getSrcPx3(srcImg, srcX, srcY, x, y);
                }
        }
        else if (srcImg.channels() == 4) {
//#pragma omp parallel for // CPU使用率太高
            for (int y = yStart; y < yEnd; y++)
                for (int x = xStart; x < xEnd; x++) {
                    const int srcX = (int)(((int64_t)x - deltaW) * curPar.ZOOM_BASE / curPar.zoomCur);
                    const int srcY = (int)(((int64_t)y - deltaH) * curPar.ZOOM_BASE / curPar.zoomCur);
                    if (0 <= srcX && srcX < srcW && 0 <= srcY && srcY < srcH)
                        ptr[y * canvasW + x] = getSrcPx4(srcImg, srcX, srcY, x, y);
                }
        }
    }

    void DrawScene() {
        const auto frameDuration = std::chrono::milliseconds(16); // 16.667 ms per frame
        
        static int64_t delayRemain = 0;
        static auto lastTimestamp = std::chrono::steady_clock::now();
        static D2D1_SIZE_U bitmapSize = D2D1::SizeU(600, 400); // 设置位图的宽度和高度

        if (m_pD2DDeviceContext == nullptr)
            return;

        auto operateAction = operateQueue.get();
        if (operateAction.action == ActionENUM::none) {
            if (curPar.zoomCur == curPar.zoomTarget &&
                curPar.slideCur == curPar.slideTarget &&
                !curPar.isAnimation) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return;
            }
        }

        switch (operateAction.action)
        {
        case ActionENUM::newSize: {
            if (operateAction.width == 0 || operateAction.height == 0)
                break;

            if (winWidth == operateAction.width && winHeight == operateAction.height)
                break;

            winWidth = operateAction.width;
            winHeight = operateAction.height;

            if (winWidth != mainCanvas.cols || winHeight != mainCanvas.rows) {
                mainCanvas = cv::Mat(winHeight, winWidth, CV_8UC4);
                bitmapSize = D2D1::SizeU(winWidth, winHeight);
                CreateWindowSizeDependentResources();
            }

            if (hasInitWinSize) {
                curPar.handleNewSize(winWidth, winHeight);
            }
            else {
                hasInitWinSize = true;
                curPar.Init(winWidth, winHeight);
            }
        } break;

        case ActionENUM::preImg: {
            if (--curFileIdx < 0)
                curFileIdx = (int)imgFileList.size() - 1;
            curPar.framesPtr = imgDB.getPtr(imgFileList[curFileIdx]);
            curPar.Init(winWidth, winHeight);
            lastTimestamp = std::chrono::steady_clock::now();
            delayRemain = 0;
        } break;

        case ActionENUM::nextImg: {
            if (++curFileIdx >= (int)imgFileList.size())
                curFileIdx = 0;
            curPar.framesPtr = imgDB.getPtr(imgFileList[curFileIdx]);
            curPar.Init(winWidth, winHeight);
            lastTimestamp = std::chrono::steady_clock::now();
            delayRemain = 0;
        } break;

        case ActionENUM::slide: {
            curPar.slideTarget.x += operateAction.x;
            curPar.slideTarget.y += operateAction.y;
        } break;

        case ActionENUM::toggleExif: {
            showExif = !showExif;
        } break;

        case ActionENUM::zoomIn: {
            if (curPar.zoomIndex > 0)
                curPar.zoomIndex--;
            auto zoomNext = curPar.zoomList[curPar.zoomIndex];
            if (curPar.zoomTarget && zoomNext != curPar.zoomTarget) {
                curPar.slideTarget.x = (int)(zoomNext * curPar.slideTarget.x / curPar.zoomTarget);
                curPar.slideTarget.y = (int)(zoomNext * curPar.slideTarget.y / curPar.zoomTarget);
            }
            curPar.zoomTarget = zoomNext;
        } break;

        case ActionENUM::zoomOut: {
            if (curPar.zoomIndex < curPar.zoomList.size() - 1)
                curPar.zoomIndex++;
            auto zoomNext = curPar.zoomList[curPar.zoomIndex];
            if (curPar.zoomTarget && zoomNext != curPar.zoomTarget) {
                curPar.slideTarget.x = (int)(zoomNext * curPar.slideTarget.x / curPar.zoomTarget);
                curPar.slideTarget.y = (int)(zoomNext * curPar.slideTarget.y / curPar.zoomTarget);
            }
            curPar.zoomTarget = zoomNext;
        } break;

        case ActionENUM::requitExit: {
            PostMessage(m_hWnd, WM_DESTROY, 0, 0);
        } break;

        default:
            break;
        }


        const auto& [srcImg, delay] = curPar.framesPtr->imgList[curPar.curFrameIdx];
        curPar.curFrameDelay = (delay <= 0 ? 10 : delay);

        if (curPar.zoomCur != curPar.zoomTarget) { // 简单缩放动画
            const int progressMax = 1 << 4;
            static int progressCnt = progressMax;
            static int64_t zoomInit = 0;
            static int64_t zoomTargetInit = 0;
            static Cood hasSlideInit{};

            //未开始进行动画 或 动画未完成就有新缩放操作
            if (progressCnt >= progressMax || zoomTargetInit != curPar.zoomTarget) {
                progressCnt = 1;
                zoomInit = curPar.zoomCur;
                zoomTargetInit = curPar.zoomTarget;
                hasSlideInit = curPar.slideCur;
            }
            else {
                auto addDelta = ((progressMax - progressCnt) / 2);
                if (addDelta <= 0) {
                    progressCnt = progressMax;
                    curPar.zoomCur = curPar.zoomTarget;
                    curPar.slideCur = curPar.slideTarget;
                }
                else {
                    progressCnt += addDelta;
                    curPar.zoomCur = zoomInit + (curPar.zoomTarget - zoomInit) * progressCnt / progressMax;
                    curPar.slideCur = hasSlideInit + (curPar.slideTarget - hasSlideInit) * progressCnt / progressMax;
                }
            }
        }
        else {
            curPar.slideCur = curPar.slideTarget;
        }

        drawCanvas(srcImg, mainCanvas);
        if (showExif) {
            const int padding = 10;
            const int rightEdge = (mainCanvas.cols - 2 * padding) / 4 + padding;
            RECT r{ padding, padding, rightEdge > 300 ? rightEdge : 300, mainCanvas.rows - padding };
            stb.putAlignLeft(mainCanvas, r, curPar.framesPtr->exifStr.c_str(), { 255, 255, 255, 255 }); // 长文本 8ms
        }

        if (showEdgeArrow == ShowEdgeArrow::left) {
            int height = mainCanvas.rows;
            int width = mainCanvas.cols;
            if (width > 100 && height > 100) {
                int triangle_height = 50;
                std::vector<cv::Point> trianglePos = {
                    cv::Point(10, height / 2),
                    cv::Point(triangle_height , height / 2 - 80),
                    cv::Point(triangle_height , height / 2 + 80)
                };
                cv::Mat overlay = cv::Mat::zeros(mainCanvas.size(), CV_8UC4);
                cv::fillConvexPoly(overlay, trianglePos, cv::Vec4b(128, 128, 128, 128));
                cv::addWeighted(overlay, 0.5, mainCanvas, 1, 0, mainCanvas);
            }
        }
        else if (showEdgeArrow == ShowEdgeArrow::right) {
            int height = mainCanvas.rows;
            int width = mainCanvas.cols;
            if (width > 100 && height > 100) {
                int triangle_height = 50;
                std::vector<cv::Point> trianglePos = {
                    cv::Point(width - 10, height / 2),
                    cv::Point(width - triangle_height , height / 2 - 80),
                    cv::Point(width - triangle_height , height / 2 + 80)
                };
                cv::Mat overlay = cv::Mat::zeros(mainCanvas.size(), CV_8UC4);
                cv::fillConvexPoly(overlay, trianglePos, cv::Vec4b(128, 128, 128, 128));
                cv::addWeighted(overlay, 0.5, mainCanvas, 1, 0, mainCanvas);
            }
        }

        wstring str = std::format(L" [{}/{}] {}% ",
            curFileIdx + 1, imgFileList.size(),
            curPar.zoomCur * 100ULL / curPar.ZOOM_BASE)
            + imgFileList[curFileIdx];
        SetWindowTextW(m_hWnd, str.c_str());

        m_pD2DDeviceContext->CreateBitmap(
            bitmapSize,
            mainCanvas.ptr(),
            (UINT32)mainCanvas.step,
            &bitmapProperties,
            &pBitmap
        );

        m_pD2DDeviceContext->BeginDraw();
        m_pD2DDeviceContext->DrawBitmap(pBitmap.Get());
        m_pD2DDeviceContext->EndDraw();
        m_pSwapChain->Present(0, 0);


        if (curPar.isAnimation) {
            if (delayRemain <= 0)
                delayRemain = curPar.curFrameDelay;

            auto nowTimestamp = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(nowTimestamp - lastTimestamp);
            lastTimestamp = nowTimestamp;

            auto remainingTime = frameDuration - elapsed;
            if (remainingTime > std::chrono::milliseconds(0)) {
                std::this_thread::sleep_for(remainingTime);
            }

            delayRemain -= elapsed.count();
            if (delayRemain <= 0) {
                delayRemain = curPar.curFrameDelay;
                curPar.curFrameIdx++;
                if (curPar.curFrameIdx > curPar.curFrameIdxMax)
                    curPar.curFrameIdx = 0;
            }
        }
    }

};

void test();

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
#ifndef NDEBUG
    AllocConsole();
    FILE* stream;
    freopen_s(&stream, "CON", "w", stdout);//重定向输入流
    freopen_s(&stream, "CON", "w", stderr);//重定向输入流

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    test();

    Exiv2::enableBMFF();
    ::ImmDisableIME(GetCurrentThreadId()); // 禁用输入法，防止干扰按键操作

    ::HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    if (!SUCCEEDED(::CoInitialize(nullptr)))
        return 0;

    JarkViewerApp app;
    if (SUCCEEDED(app.Initialize(hInstance, nCmdShow, lpCmdLine)))
        app.Run();

    ::CoUninitialize();
    return 0;
}

void test() {
    return;

    std::ifstream file("D:\\Downloads\\test\\22.wp2", std::ios::binary);
    auto buf = std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});

    exit(0);
}
