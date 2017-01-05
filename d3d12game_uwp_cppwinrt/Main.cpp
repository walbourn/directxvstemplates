//
// Main.cpp
//

#include "pch.h"
#include "Game.h"

using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Input;
using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display;
using namespace DirectX;

class ViewProvider final : public winrt::implements<ViewProvider, IFrameworkView>
{
public:
    ViewProvider() :
        m_exit(false),
        m_visible(true),
        m_DPI(96.f),
        m_logicalWidth(800.f),
        m_logicalHeight(600.f),
        m_nativeOrientation(DisplayOrientations::None),
        m_currentOrientation(DisplayOrientations::None)
    {
    }

    // IFrameworkView methods
    void Initialize(CoreApplicationView const & applicationView)
    {
        applicationView.Activated({ this, &ViewProvider::OnActivated });

        CoreApplication::Suspending({ this, &ViewProvider::OnSuspending });

        CoreApplication::Resuming({ this, &ViewProvider::OnResuming });

        m_game = std::make_unique<Game>();
    }

    void Uninitialize()
    {
        m_game.reset();
    }

    void SetWindow(CoreWindow const & window)
    {
        window.SizeChanged({ this, &ViewProvider::OnWindowSizeChanged });

        window.VisibilityChanged({ this, &ViewProvider::OnVisibilityChanged });

        window.Closed({ this, &ViewProvider::OnWindowClosed });

        auto dispatcher = CoreWindow::GetForCurrentThread().Dispatcher();

        dispatcher.AcceleratorKeyActivated({ this, &ViewProvider::OnAcceleratorKeyActivated });

        auto currentDisplayInformation = DisplayInformation::GetForCurrentView();

        currentDisplayInformation.DpiChanged({ this, &ViewProvider::OnDpiChanged });

        currentDisplayInformation.OrientationChanged({ this, &ViewProvider::OnOrientationChanged });

        DisplayInformation::DisplayContentsInvalidated({ this, &ViewProvider::OnDisplayContentsInvalidated });

        m_DPI = currentDisplayInformation.LogicalDpi();

        m_logicalWidth = window.Bounds().Width;
        m_logicalHeight = window.Bounds().Height;

        m_nativeOrientation = currentDisplayInformation.NativeOrientation();
        m_currentOrientation = currentDisplayInformation.CurrentOrientation();

        int outputWidth = ConvertDipsToPixels(m_logicalWidth);
        int outputHeight = ConvertDipsToPixels(m_logicalHeight);

        DXGI_MODE_ROTATION rotation = ComputeDisplayRotation();

        if (rotation == DXGI_MODE_ROTATION_ROTATE90 || rotation == DXGI_MODE_ROTATION_ROTATE270)
        {
            std::swap(outputWidth, outputHeight);
        }

        m_game->Initialize(winrt::get(window), outputWidth, outputHeight, rotation);
    }

    void Load(winrt::hstring const &)
    {
    }

    void Run()
    {
        while (!m_exit)
        {
            if (m_visible)
            {
                m_game->Tick();

                CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
            }
            else
            {
                CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
            }
        }
    }

protected:
    // Event handlers
    void OnActivated(CoreApplicationView const & /*applicationView*/, IActivatedEventArgs const & args)
    {
        if (args.Kind() == ActivationKind::Launch)
        {
            auto launchArgs = (const LaunchActivatedEventArgs*)(&args);

            if (launchArgs->PrelaunchActivated())
            {
                // Opt-out of Prelaunch
                CoreApplication::Exit();
                return;
            }
        }

        int w, h;
        m_game->GetDefaultSize(w, h);

        m_DPI = DisplayInformation::GetForCurrentView().LogicalDpi();

        ApplicationView::PreferredLaunchWindowingMode(ApplicationViewWindowingMode::PreferredLaunchViewSize);
        // Change to ApplicationViewWindowingMode::FullScreen to default to full screen

        auto desiredSize = Size(ConvertPixelsToDips(w), ConvertPixelsToDips(h));

        ApplicationView::PreferredLaunchViewSize(desiredSize);

        auto view = ApplicationView::GetForCurrentView();

        auto minSize = Size(ConvertPixelsToDips(320), ConvertPixelsToDips(200));

        view.SetPreferredMinSize(minSize);

        CoreWindow::GetForCurrentThread().Activate();

        view.FullScreenSystemOverlayMode(FullScreenSystemOverlayMode::Minimal);

        view.TryResizeView(desiredSize);
    }

    void OnSuspending(IInspectable const & /*sender*/, SuspendingEventArgs const & args)
    {
        auto deferral = args.SuspendingOperation().GetDeferral();

        std::async(std::launch::async, [this, deferral]()
        {
            m_game->OnSuspending();

            deferral.Complete();
        });
    }

    void OnResuming(IInspectable const & /*sender*/, IInspectable const & /*args*/)
    {
        m_game->OnResuming();
    }

    void OnWindowSizeChanged(CoreWindow const & sender, WindowSizeChangedEventArgs const & /*args*/)
    {
        m_logicalWidth = sender.Bounds().Width;
        m_logicalHeight = sender.Bounds().Height;

        HandleWindowSizeChanged();
    }

    void OnVisibilityChanged(CoreWindow const & /*sender*/, VisibilityChangedEventArgs const & args)
    {
        m_visible = args.Visible();
        if (m_visible)
            m_game->OnActivated();
        else
            m_game->OnDeactivated();
    }

    void OnWindowClosed(CoreWindow const & /*sender*/, CoreWindowEventArgs const & /*args*/)
    {
        m_exit = true;
    }

    void OnAcceleratorKeyActivated(CoreDispatcher const &, AcceleratorKeyEventArgs const & args)
    {
        if (args.EventType() == CoreAcceleratorKeyEventType::SystemKeyDown
            && args.VirtualKey() == VirtualKey::Enter
            && args.KeyStatus().IsMenuKeyDown
            && !args.KeyStatus().WasKeyDown)
        {
            // Implements the classic ALT+ENTER fullscreen toggle
            auto view = ApplicationView::GetForCurrentView();

            if (view.IsFullScreenMode())
                view.ExitFullScreenMode();
            else
                view.TryEnterFullScreenMode();

            args.Handled(true);
        }
    }

    void OnDpiChanged(DisplayInformation const & sender, IInspectable const & /*args*/)
    {
        m_DPI = sender.LogicalDpi();

        HandleWindowSizeChanged();
    }

    void OnOrientationChanged(DisplayInformation const & sender, IInspectable const & /*args*/)
    {
        m_currentOrientation = sender.CurrentOrientation();

        HandleWindowSizeChanged();
    }

    void OnDisplayContentsInvalidated(DisplayInformation const & /*sender*/, IInspectable const & /*args*/)
    {
        m_game->ValidateDevice();
    }

private:
    bool                    m_exit;
    bool                    m_visible;
    float                   m_DPI;
    float                   m_logicalWidth;
    float                   m_logicalHeight;
    std::unique_ptr<Game>   m_game;

    winrt::Windows::Graphics::Display::DisplayOrientations	m_nativeOrientation;
    winrt::Windows::Graphics::Display::DisplayOrientations	m_currentOrientation;

    inline int ConvertDipsToPixels(float dips) const
    {
        return int(dips * m_DPI / 96.f + 0.5f);
    }

    inline float ConvertPixelsToDips(int pixels) const
    {
        return (float(pixels) * 96.f / m_DPI);
    }

    DXGI_MODE_ROTATION ComputeDisplayRotation() const
    {
        DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

        switch (m_nativeOrientation)
        {
        case DisplayOrientations::Landscape:
            switch (m_currentOrientation)
            {
            case DisplayOrientations::Landscape:
                rotation = DXGI_MODE_ROTATION_IDENTITY;
                break;

            case DisplayOrientations::Portrait:
                rotation = DXGI_MODE_ROTATION_ROTATE270;
                break;

            case DisplayOrientations::LandscapeFlipped:
                rotation = DXGI_MODE_ROTATION_ROTATE180;
                break;

            case DisplayOrientations::PortraitFlipped:
                rotation = DXGI_MODE_ROTATION_ROTATE90;
                break;
            }
            break;

        case DisplayOrientations::Portrait:
            switch (m_currentOrientation)
            {
            case DisplayOrientations::Landscape:
                rotation = DXGI_MODE_ROTATION_ROTATE90;
                break;

            case DisplayOrientations::Portrait:
                rotation = DXGI_MODE_ROTATION_IDENTITY;
                break;

            case DisplayOrientations::LandscapeFlipped:
                rotation = DXGI_MODE_ROTATION_ROTATE270;
                break;

            case DisplayOrientations::PortraitFlipped:
                rotation = DXGI_MODE_ROTATION_ROTATE180;
                break;
            }
            break;
        }

        return rotation;
    }

    void HandleWindowSizeChanged()
    {
        int outputWidth = ConvertDipsToPixels(m_logicalWidth);
        int outputHeight = ConvertDipsToPixels(m_logicalHeight);

        DXGI_MODE_ROTATION rotation = ComputeDisplayRotation();

        if (rotation == DXGI_MODE_ROTATION_ROTATE90 || rotation == DXGI_MODE_ROTATION_ROTATE270)
        {
            std::swap(outputWidth, outputHeight);
        }

        m_game->OnWindowSizeChanged(outputWidth, outputHeight, rotation);
    }
};

class ViewProviderFactory final : public winrt::implements<ViewProviderFactory, IFrameworkViewSource>
{
public:
    IFrameworkView CreateView()
    {
        return winrt::make<ViewProvider>();
    }
};


// Entry point
int CALLBACK WinMain(
    _In_ HINSTANCE /*hInstance*/,
    _In_ HINSTANCE /*hPrevInstance*/,
    _In_ LPSTR     /*lpCmdLine*/,
    _In_ int       /*nCmdShow*/
)
{
    if (!XMVerifyCPUSupport())
    {
        throw std::exception("XMVerifyCPUSupport");
    }

    ViewProviderFactory viewProviderFactory;
    CoreApplication::Run(viewProviderFactory);
    return 0;
}


// Exit helper
void ExitGame()
{
    winrt::Windows::ApplicationModel::Core::CoreApplication::Exit();
}