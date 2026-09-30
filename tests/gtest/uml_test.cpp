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
