#include <QApplication>
#include <QTimer>

#include <nativefx/nativefx_server.hpp>
#include "vfxwebpage.h"

// maos wants to force us to take a nap.
// this is how we prevent that appnap stuff.
#ifdef __APPLE__
// forward declaration
void __macos__disable_app_nap();
#endif // __APPLE__

namespace nfx = nativefx;

int parseArgs(int argc, char** argv, std::string &mem_name, std::string &url) {

    args::ArgumentParser parser("This is a NativeFX server program.", "---");
    args::HelpFlag helpArg(parser, "help", "Display this help menu", {'h', "help"});
    args::ValueFlag<std::string> nameArg(parser, "name", "Defines the name of the shared memory objects to be created by this program", {'n', "name"});
    args::ValueFlag<std::string> urlArg(parser, "url", "Defines the url to navigate to", {"url"});
    args::Flag deleteSharedMem(parser, "delete", "Indicates that existing shared memory with the specified name should be deleted", {'d', "delete"});

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (const args::Completion& e)
    {
        std::cout << e.what();
        return nfx::SUCCESS;
    }
    catch (const args::Help&)
    {
        std::cerr << parser;
        return nfx::ERROR | nfx::ARGS_ERROR;
    }
    catch (const args::ParseError& e)
    {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return nfx::ERROR | nfx::ARGS_ERROR;
    }

    std::string _name = args::get(nameArg);

    if(_name.size() == 0) {
        std::cerr << std::endl << std::endl << "ERROR: 'name' must be specified to create or delete shared memory!" << std::endl << std::endl;
        std::cerr << parser;
        return nfx::ERROR | nfx::ARGS_ERROR;
    }

    std::string _url = args::get(urlArg);

    // if(_url.size() == 0) {
    //     std::cerr << std::endl << std::endl << "ERROR: 'url' must be specified to use this server!" << std::endl << std::endl;
    //     std::cerr << parser;
    //     return nfx::ERROR | nfx::ARGS_ERROR;
    // }

    mem_name = _name;
    url      = _url;

    if(deleteSharedMem) {
        // remove shared memory objects
        return nfx::delete_shared_mem(_name);
    }

    return nfx::SUCCESS;
}

int main(int argc, char *argv[])
{

#ifdef __APPLE__
    __macos__disable_app_nap();
#endif // __APPLE__    

    // create qapplication before touching any other qobject/class
    QApplication app(argc, argv, false);

    QImage* img = NULL;

    QWebEngineSettings::globalSettings()->setAttribute(QWebEngineSettings::WebGLEnabled, true);
    // http://stackoverflow.com/questions/29773047/using-qwebengine-to-render-an-image
    //std::cout << "QT_VERSION: " << QT_VERSION << std::endl;
    
    VFXWebPage webView;

    auto redraw = [&img, &webView](std::string const& name, uchar* buffer_data, int W, int H) {
        if(img == NULL) {
            //std::cout << "new img\n";
            // TODO do we need to delete qimage?
            img = new QImage(buffer_data, W,H, W*4, QImage::Format_ARGB32_Premultiplied);
           //webView.request_redraw(NULL);
            webView.resize(W, H);
            webView.request_redraw(img);
        }
    };

    auto resized = [&img, &webView](std::string const& name, uchar* buffer_data, int W, int H) {
        if(img == NULL) {
            //std::cout << "new img\n";
            // TODO do we need to delete qimage?
            img = new QImage(buffer_data, W,H, W*4, QImage::Format_ARGB32_Premultiplied);
           //webView.request_redraw(NULL);
            webView.resize(W, H);
            webView.request_redraw(img);
        } else if(img->width() != W || img->height() != H) {
            //std::cout << "new img\n";
            // TODO do we need to delete qimage?
            img = new QImage(buffer_data, W,H, W*4, QImage::Format_ARGB32_Premultiplied);
            //webView.request_redraw(NULL);
            webView.resize(W, H);
            webView.request_redraw(img);
        }
    };

    std::string mem_name;
    std::string url;

    int res = parseArgs(argc,argv, mem_name, url);
    if(res != nfx::SUCCESS) {
        return res;
    }

    nfx::shared_canvas *canvas = nfx::shared_canvas::create(mem_name);

    QWidget* prevEvtTarget = NULL; 
    QPoint prevP;

    auto evt = [&webView, &prevEvtTarget, &prevP](std::string const &name, nfx::event* evt) {
        // std::cout << "[" + name + "] " << "event received: type=" << evt->type << ", \n";

        if(!(evt->type & nfx::MOUSE_EVENT)) {
            std::cout << "-> not a mouse event!\n";
            return; // this is not a mouse event
        }

        nfx::mouse_event* mouse_evt = static_cast<nfx::mouse_event*>((void*)evt);

        QPoint p(mouse_evt->x,mouse_evt->y);

        // std::cout << " -> location: x: " << mouse_evt->x << ", y: " << mouse_evt->y << "\n";

        Qt::MouseButton btn = Qt::NoButton;

        QWidget *widget  = QApplication::widgetAt(p);
        QWidget *receiver  = NULL;
        if(widget==NULL) {
           receiver= webView.childAt(p);
        } else {
           receiver= widget->childAt(p);
        }
        
        if(receiver==NULL) {
            return;
        }

        // detected mouse enter/exit events
        if(prevEvtTarget!=receiver) {
            
                if(prevEvtTarget!=NULL) {
                    std::cout << "LEAVE\n";
                    QMouseEvent* mEvt = new QMouseEvent( (QEvent::Leave), prevP, 
                        Qt::NoButton,
                        Qt::NoButton,
                        Qt::NoModifier  
                    );
                    QApplication::sendEvent(prevEvtTarget, mEvt); 
                }

                std::cout << "ENTER\n";
                QMouseEvent* mEvt = new QMouseEvent( (QEvent::Enter), p, 
                        Qt::NoButton,
                        Qt::NoButton,
                        Qt::NoModifier  
                );
                QApplication::sendEvent(receiver, mEvt); 
        }

        prevEvtTarget = receiver;
        prevP = p;

        if(mouse_evt->buttons & nfx::PRIMARY_BTN) {
            btn = Qt::LeftButton;
            //std::cout << "-> btn: PRIMARY\n";
        }

        if(mouse_evt->buttons & nfx::SECONDARY_BTN) {
            btn = Qt::RightButton;
            //std::cout << "-> btn: SECONDARY\n";
        }

        if(mouse_evt->buttons & nfx::MIDDLE_BTN) {
            btn = Qt::MiddleButton;
            //std::cout << "-> btn: MIDDLE\n";
        }

        if(mouse_evt->type & nfx::MOUSE_MOVED) {
            QMouseEvent* mEvt = new QMouseEvent( (QEvent::MouseMove), p, 
                Qt::NoButton,
                Qt::NoButton,
                Qt::NoModifier  
            );
            //std::cout << "-> evt-type: MOVE\n";
            QApplication::sendEvent(receiver, mEvt);
        } 
        
        if(mouse_evt->type & nfx::MOUSE_PRESSED) {
            QMouseEvent* mEvt = new QMouseEvent( (QEvent::MouseButtonPress), p, 
                btn,
                Qt::NoButton,
                Qt::NoModifier
            );
            //std::cout << "-> evt-type: PRESS\n";
            QApplication::sendEvent(receiver, mEvt);
        } 
        
        if(mouse_evt->type & nfx::MOUSE_RELEASED) {
            QMouseEvent* mEvt = new QMouseEvent( (QEvent::MouseButtonRelease), p, 
                btn,
                Qt::NoButton,
                Qt::NoModifier   
            );
            //std::cout << "-> evt-type: RELEASE\n";
            QApplication::sendEvent(receiver, mEvt);
        }

    }; // end evt

    QTimer* timer = new QTimer;
    QObject::connect(timer, &QTimer::timeout, [&canvas,&evt](){
        canvas->process_events(evt);
    });
    timer->setSingleShot(false);
    timer->start(100);

    auto redraw_1 = [&canvas, &redraw, &resized, &evt]() {
        canvas->process_events(evt);
        canvas->draw(redraw,resized);
    };

    webView.set_redraw_callback(redraw_1);

    webView.setAttribute(Qt::WA_DontShowOnScreen);
    // install event filter on itself TODO maybe problematic since
    // the same filter is registered multiple times for GL children
    webView.installEventFilter(&webView);
    webView.resize(1024,768);
    if(url.size()!=0) {
        webView.load(QUrl(url.c_str()));
    } else {
        webView.load(QUrl("http://carvisualizer.plus360degrees.com/threejs/"));
    }
    // webView.load(QUrl("http://carvisualizer.plus360degrees.com/threejs/"));
    // webView.load(QUrl("http://carvisualizer.plus360degrees.com/threejs/"));
    // webView.load(QUrl("http://mihosoft.eu"));
    // webView.load(QUrl("http://youtube.com"));
    webView.show();
 
    // qapplication does its magic...
    int exec_result = app.exec();

    return exec_result;
}