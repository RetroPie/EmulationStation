#include "components/TextComponent.h"

#include "utils/StringUtil.h"
#include "Log.h"
#include "Settings.h"

TextComponent::TextComponent(Window* window) : GuiComponent(window),
    mFont(Font::get(FONT_SIZE_MEDIUM)), mUppercase(false),
    mColor(0xFFFFFFFF), mBgColor(0x00000000),
    mStrokeColor(0x000000FF), mColorOpacity(0xFF),
    mBgColorOpacity(0x00), mStrokeColorOpacity(0xFF),
    mRenderBackground(false),
    mStrokeSize(0.0f),   // *** SIN BORDE POR DEFECTO ***
    mAutoCalcExtent(true, true),
    mHorizontalAlignment(ALIGN_LEFT),
    mVerticalAlignment(ALIGN_CENTER),
    mLineSpacing(1.5f)
{
}

TextComponent::TextComponent(Window* window, const std::string& text, const std::shared_ptr<Font>& font,
    unsigned int color, Alignment align, Vector3f pos, Vector2f size, unsigned int bgcolor)
    : GuiComponent(window),
    mFont(nullptr), mUppercase(false), mColor(color), mBgColor(bgcolor),
    mStrokeColor(0x000000FF), mColorOpacity(color & 0xFF),
    mBgColorOpacity(bgcolor & 0xFF), mStrokeColorOpacity(0xFF),
    mRenderBackground(bgcolor != 0), // activar si hay fondo
    mAutoCalcExtent(true, true),
    mHorizontalAlignment(align), mVerticalAlignment(ALIGN_CENTER),
    mLineSpacing(1.5f),
    mStrokeSize(0.0f) // sin borde
{
    setFont(font);
    setText(text);
    setPosition(pos);
    setSize(size);
}

void TextComponent::onSizeChanged()
{
    mAutoCalcExtent = Vector2i((getSize().x() == 0), (getSize().y() == 0));
    onTextChanged();
}

void TextComponent::setFont(const std::shared_ptr<Font>& font)
{
    mFont = font;
    onTextChanged();
}

void TextComponent::setColor(unsigned int color)
{
    mColor = color;
    mColorOpacity = (color & 0xFF);
    onColorChanged();
}

void TextComponent::setBackgroundColor(unsigned int color)
{
    mBgColor = color;
    mBgColorOpacity = (color & 0xFF);

    // activar fondo automáticamente si el color no es transparente
    mRenderBackground = (mBgColorOpacity > 0);
}

void TextComponent::setRenderBackground(bool render)
{
    mRenderBackground = render;
}

void TextComponent::setTextStrokeColor(unsigned int color)
{
    mStrokeColor = color;
    mStrokeColorOpacity = (color & 0xFF);
}

void TextComponent::setTextStrokeSize(float size)
{
    mStrokeSize = size;
}

void TextComponent::setOpacity(unsigned char opacity)
{
    unsigned char o = (unsigned char)((float)opacity / 255.f * (float)mColorOpacity);
    mColor = (mColor & 0xFFFFFF00) | o;

    unsigned char bgo = (unsigned char)((float)opacity / 255.f * (float)mBgColorOpacity);
    mBgColor = (mBgColor & 0xFFFFFF00) | bgo;

    unsigned char so = (unsigned char)((float)opacity / 255.f * (float)mStrokeColorOpacity);
    mStrokeColor = (mStrokeColor & 0xFFFFFF00) | so;

    onColorChanged();

    GuiComponent::setOpacity(opacity);
}

unsigned char TextComponent::getOpacity() const
{
    return (mColor & 0xFF);
}

void TextComponent::setText(const std::string& text)
{
    mText = text;
    onTextChanged();
}

void TextComponent::setUppercase(bool uppercase)
{
    mUppercase = uppercase;
    onTextChanged();
}

void TextComponent::setHorizontalAlignment(Alignment align)
{
    mHorizontalAlignment = align;
    onTextChanged();
}

void TextComponent::setVerticalAlignment(Alignment align)
{
    mVerticalAlignment = align;
}

void TextComponent::setLineSpacing(float spacing)
{
    mLineSpacing = spacing;
    onTextChanged();
}

void TextComponent::setValue(const std::string& value)
{
    setText(value);
}

std::string TextComponent::getValue() const
{
    return mText;
}

void TextComponent::render(const Transform4x4f& parentTrans)
{
    if (!isVisible() || !mFont)
        return;

    Transform4x4f trans = parentTrans * getTransform();
    Renderer::setMatrix(trans);

    // ===============================
    //  FONDO DEL TEXTO SI CORRESPONDE
    // ===============================
    if (mRenderBackground && (mBgColorOpacity > 0))
    {
        Renderer::drawRect(0, 0, mSize.x(), mSize.y(), mBgColor, mBgColor);
    }

    if (!mTextCache)
        return;

    const Vector2f& size = mTextCache->metrics.size;

    float yOff = 0.0f;
    switch (mVerticalAlignment)
    {
    case ALIGN_TOP:    yOff = 0; break;
    case ALIGN_BOTTOM: yOff = (getSize().y() - size.y()); break;
    case ALIGN_CENTER: yOff = (getSize().y() - size.y()) / 2.0f; break;
    }

    Transform4x4f baseTrans = trans;
    baseTrans.translate(Vector3f(0.0f, yOff, 0.0f));

    // ===============================
    //       STROKE / BORDE
    // ===============================
    if (mStrokeSize > 0.0f && (mStrokeColorOpacity > 0))
    {
        unsigned int originalColor = mColor;

        mTextCache->setColor(mStrokeColor);

        const float s = mStrokeSize;

        const Vector2f offsets[4] = {
            Vector2f(-s, 0),
            Vector2f( s, 0),
            Vector2f(0, -s),
            Vector2f(0,  s)
        };

        for (int i = 0; i < 4; i++)
        {
            Transform4x4f strokeTrans = baseTrans;
            strokeTrans.translate(Vector3f(offsets[i].x(), offsets[i].y(), 0));
            Renderer::setMatrix(strokeTrans);
            mFont->renderTextCache(mTextCache.get());
        }

        mTextCache->setColor(originalColor);
    }

    // ===============================
    //        TEXTO PRINCIPAL
    // ===============================
    Renderer::setMatrix(baseTrans);
    mFont->renderTextCache(mTextCache.get());
}

std::string TextComponent::calculateExtent(bool allow_wrapping)
{
    std::string text = mUppercase ? Utils::String::toUpper(mText) : mText;

    if (mAutoCalcExtent.x())
    {
        mSize = mFont->sizeText(text, mLineSpacing);
    }
    else if (mAutoCalcExtent.y() || allow_wrapping)
    {
        text = mFont->wrapText(text, getSize().x());
        if (mAutoCalcExtent.y())
            mSize.y() = mFont->sizeText(text, mLineSpacing).y();
    }

    return text;
}

void TextComponent::onTextChanged()
{
    if (!mFont || mText.empty())
    {
        mTextCache.reset();
        return;
    }

    std::string text = calculateExtent(mSize.y() > mFont->getHeight(mLineSpacing));
    const bool oneLine = (mSize.y() > 0 && mSize.y() <= mFont->getHeight(mLineSpacing));

    if (oneLine)
    {
        bool shorten = false;
        size_t newline = text.find('\n');
        text = text.substr(0, newline);
        Vector2f ts = mFont->sizeText(text);
        shorten = newline != std::string::npos || ts.x() > mSize.x();

        if (shorten)
        {
            const std::string dots = "...";
            Vector2f dotsSize = mFont->sizeText(dots);

            while (text.size() && (ts.x() + dotsSize.x() > mSize.x()))
            {
                size_t newSize = Utils::String::prevCursor(text, text.size());
                text.erase(newSize);
                ts = mFont->sizeText(text);
            }
            text.append(dots);
        }
    }

    mTextCache = std::shared_ptr<TextCache>(
        mFont->buildTextCache(
            text,
            Vector2f(0, 0),
            (mColor >> 8 << 8) | mOpacity,
            mSize.x(),
            mHorizontalAlignment,
            mLineSpacing
        ));
}

void TextComponent::onColorChanged()
{
    if (mTextCache)
        mTextCache->setColor(mColor);
}

void TextComponent::applyTheme(const std::shared_ptr<ThemeData>& theme,
    const std::string& view, const std::string& element, unsigned int properties)
{
    GuiComponent::applyTheme(theme, view, element, properties);

    using namespace ThemeFlags;
    const ThemeData::ThemeElement* elem = theme->getElement(view, element, "text");
    if (!elem)
        return;

    if (properties & COLOR && elem->has("color"))
        setColor(elem->get<unsigned int>("color"));

    if (elem->has("backgroundColor"))
        setBackgroundColor(elem->get<unsigned int>("backgroundColor"));

    if (properties & ALIGNMENT && elem->has("alignment"))
    {
        std::string a = elem->get<std::string>("alignment");
        if (a == "left")   setHorizontalAlignment(ALIGN_LEFT);
        else if (a == "center") setHorizontalAlignment(ALIGN_CENTER);
        else if (a == "right")  setHorizontalAlignment(ALIGN_RIGHT);
    }

    if (properties & TEXT && elem->has("text"))
        setText(elem->get<std::string>("text"));

    if (properties & FORCE_UPPERCASE && elem->has("forceUppercase"))
        setUppercase(elem->get<bool>("forceUppercase"));

    if (properties & LINE_SPACING && elem->has("lineSpacing"))
        setLineSpacing(elem->get<float>("lineSpacing"));

    // STROKE (borde de texto)
    if (elem->has("textStrokeColor"))
        setTextStrokeColor(elem->get<unsigned int>("textStrokeColor"));

    if (elem->has("textStrokeSize"))
        setTextStrokeSize(elem->get<float>("textStrokeSize"));

    setFont(Font::getFromTheme(elem, properties, mFont));
}
