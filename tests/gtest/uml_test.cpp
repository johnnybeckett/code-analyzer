#include <gtest/gtest.h>
#include "uml/template_renderer.h"
#include <string>

// The splice must never re-scan an emitted value: a replacement that itself
// contains a placeholder token is inserted verbatim and the token survives.
TEST(TemplateRendererTest, EmittedValueContainingTokenIsNotRescanned) {
    std::string doc = "start __TOK__ end";
    uml::TemplateRenderer::spliceAll(doc, {{"__TOK__", "value with __TOK__ inside"}});
    EXPECT_EQ(doc, "start value with __TOK__ inside end");
}

TEST(TemplateRendererTest, MultipleSubstitutionsInOnePass) {
    std::string doc = "A __ONE__ mid __TWO__";
    uml::TemplateRenderer::spliceAll(doc, {{"__ONE__", "1"}, {"__TWO__", "2"}});
    EXPECT_EQ(doc, "A 1 mid 2");
}

TEST(TemplateRendererTest, UntouchedTextPassesThrough) {
    std::string doc = "no tokens here";
    uml::TemplateRenderer::spliceAll(doc, {{"__TOK__", "x"}});
    EXPECT_EQ(doc, "no tokens here");
}

TEST(TemplateRendererTest, RenderSplicesTheRealTokens) {
    uml::TemplateRenderer renderer;
    const std::string page = renderer.render({
        { "__DIFF_MODE__", "false" },
        { "__OLD_CLASSES_JSON__", "[]" },
        { "__NEW_CLASSES_JSON__", "[{\"name\":\"X\"}]" },
    });

    EXPECT_NE(page.find("<!DOCTYPE html>"), std::string::npos);
    EXPECT_NE(page.find("const DIFF_MODE = false;"), std::string::npos);
    EXPECT_NE(page.find("const OLD_CLASSES = [];"), std::string::npos);
    EXPECT_NE(page.find("const NEW_CLASSES = [{\"name\":\"X\"}];"), std::string::npos);
    // No placeholder token may survive the render
    EXPECT_EQ(page.find("__DIFF_MODE__"), std::string::npos);
    EXPECT_EQ(page.find("__OLD_CLASSES_JSON__"), std::string::npos);
    EXPECT_EQ(page.find("__NEW_CLASSES_JSON__"), std::string::npos);
}

// Mobile / PC detection: the page must ship both input models — the existing
// mouse model for desktop, and a touch model (gesture handlers + on-screen
// button pad) that is activated only on coarse pointers. ctest cannot run
// the JS, so the presence and wiring of both models is the contract.
TEST(TemplateRendererTest, ShipsTouchModelAndKeepsMouseModel) {
    uml::TemplateRenderer renderer;
    const std::string page = renderer.render({
        { "__DIFF_MODE__", "false" },
        { "__OLD_CLASSES_JSON__", "[]" },
        { "__NEW_CLASSES_JSON__", "[]" },
    });

    // Coarse-pointer detection flags the document and swaps the hint.
    EXPECT_NE(page.find("maxTouchPoints"), std::string::npos);
    EXPECT_NE(page.find("(pointer: coarse)"), std::string::npos);
    EXPECT_NE(page.find("classList.add('touch')"), std::string::npos);

    // Touch gestures: one-finger orbit, pinch zoom, two-finger pan.
    EXPECT_NE(page.find("'touchstart'"), std::string::npos);
    EXPECT_NE(page.find("'touchmove'"), std::string::npos);
    EXPECT_NE(page.find("'touchend'"), std::string::npos);

    // The on-screen pad and its actions (move / zoom / nav / center).
    EXPECT_NE(page.find("id=\"touchpad\""), std::string::npos);
    for (const char* act : {"zoomin", "zoomout", "center", "prev", "next",
                            "up", "down", "left", "right"}) {
        EXPECT_NE(page.find(std::string("data-act=\"") + act + "\""), std::string::npos)
            << "missing pad action " << act;
    }
    // Hold-to-move buttons ride the same heldKeys / moveView path as WASD.
    EXPECT_NE(page.find("heldKeys.add"), std::string::npos);

    // Visibility rule: hidden by default, shown only when flagged touch.
    EXPECT_NE(page.find("body.touch #touchpad { display: block; }"), std::string::npos);

    // The mouse model stays intact for PC, and is disabled on touch.
    EXPECT_NE(page.find("'mousedown'"), std::string::npos);
    EXPECT_NE(page.find("if (touchMode) return;"), std::string::npos);
}
