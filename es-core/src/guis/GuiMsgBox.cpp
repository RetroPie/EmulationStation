// es-core/src/guis/GuiMsgBox.cpp

#include "guis/GuiMsgBox.h"

#include "components/ButtonComponent.h"
#include "components/MenuComponent.h"
#include "../LocaleESHook.h"  // Para es_translate

#define HORIZONTAL_PADDING_PX 20

GuiMsgBox::GuiMsgBox(Window* window, const std::string& text,
    const std::string& name1, const std::function<void()>& func1,
    const std::string& name2, const std::function<void()>& func2,
    const std::string& name3, const std::function<void()>& func3)
    : GuiComponent(window)
    , mBackground(window, ":/frame.png")
    , mGrid(window, Vector2i(1, 2))
{
    float width    = Renderer::getScreenWidth() * 0.6f; // max width
    float minWidth = Renderer::getScreenWidth() * 0.3f; // minimum width

    // Texto del mensaje traducido
    mMsg = std::make_shared<TextComponent>(
        mWindow,
        es_translate(text),
        Font::get(FONT_SIZE_MEDIUM),
        0x777777FF,
        ALIGN_CENTER
    );
    mGrid.setEntry(mMsg, Vector2i(0, 0), false, false);

    // Botones traducidos
    if (!name1.empty())
        mButtons.push_back(std::make_shared<ButtonComponent>(
            mWindow,
            es_translate(name1),
            name1,
            std::bind(&GuiMsgBox::deleteMeAndCall, this, func1)));

    if (!name2.empty())
        mButtons.push_back(std::make_shared<ButtonComponent>(
            mWindow,
            es_translate(name2),
            name2,
            std::bind(&GuiMsgBox::deleteMeAndCall, this, func2)));

    if (!name3.empty())
        mButtons.push_back(std::make_shared<ButtonComponent>(
            mWindow,
            es_translate(name3),
            name3,
            std::bind(&GuiMsgBox::deleteMeAndCall, this, func3)));

    // Botón “acelerador” (B / Enter / etc.)
    if (mButtons.size() == 1)
    {
        mAcceleratorFunc = mButtons.front()->getPressedFunc();
    }
    else
    {
        for (auto& btn : mButtons)
        {
            std::string txtUpper = Utils::String::toUpper(btn->getText());
            if (txtUpper == es_translate("OK") || txtUpper == es_translate("NO") ||
                txtUpper == "OK" || txtUpper == "NO")
            {
                mAcceleratorFunc = btn->getPressedFunc();
                break;
            }
        }
    }

    // Grid de botones
    mButtonGrid = makeButtonGrid(mWindow, mButtons);
    mGrid.setEntry(mButtonGrid, Vector2i(0, 1), true, false,
                   Vector2i(1, 1), GridFlags::BORDER_TOP);

    // Ajuste de ancho
    if (mMsg->getSize().x() < width && mButtonGrid->getSize().x() < width)
    {
        width = Math::max(mButtonGrid->getSize().x(), mMsg->getSize().x());
        width = Math::max(width, minWidth);
    }

    mMsg->setSize(width, 0);
    const float msgHeight = Math::max(
        Font::get(FONT_SIZE_LARGE)->getHeight(),
        mMsg->getSize().y() * 1.225f
    );

    setSize(width + HORIZONTAL_PADDING_PX * 2,
            msgHeight + mButtonGrid->getSize().y());

    setPosition(
        (Renderer::getScreenWidth()  - mSize.x()) / 2.0f,
        (Renderer::getScreenHeight() - mSize.y()) / 2.0f
    );

    addChild(&mBackground);
    addChild(&mGrid);
}

bool GuiMsgBox::input(InputConfig* config, Input input)
{
    if (config->getDeviceId() == DEVICE_KEYBOARD && !config->isConfigured() &&
        input.value &&
        (input.id == SDLK_RETURN || input.id == SDLK_ESCAPE || input.id == SDLK_SPACE))
    {
        if (mAcceleratorFunc)
            mAcceleratorFunc();
        return true;
    }

    if (mAcceleratorFunc && config->isMappedTo("b", input) && input.value != 0)
    {
        mAcceleratorFunc();
        return true;
    }

    return GuiComponent::input(config, input);
}

void GuiMsgBox::onSizeChanged()
{
    mGrid.setSize(mSize);
    mGrid.setRowHeightPerc(1, mButtonGrid->getSize().y() / mSize.y());

    mMsg->setSize(mSize.x() - HORIZONTAL_PADDING_PX * 2, mGrid.getRowHeight(0));
    mGrid.onSizeChanged();

    mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
}

void GuiMsgBox::deleteMeAndCall(const std::function<void()>& func)
{
    auto funcCopy = func;
    delete this;
    if (funcCopy)
        funcCopy();
}

std::vector<HelpPrompt> GuiMsgBox::getHelpPrompts()
{
    return mGrid.getHelpPrompts();
}
