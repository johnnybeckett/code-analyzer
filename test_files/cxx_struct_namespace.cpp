// Fixture exercising class/struct/union kind capture across namespace
// variants: nested namespaces, a C++17 qualified namespace, an anonymous
// namespace, the global namespace, and a template class.
//
// Expected capture (name / kind / namespace):
//   NestedStruct  struct  one::two
//   NestedClass   class   one::two
//   NestedUnion   union   one
//   DeepClass     class   one::four::five
//   AnonStruct    struct  (anonymous)
//   Templated     class   (global)
//   PlainStruct   struct  (global)
//
// Expected NON-capture:
//   Color, Prio          scoped enums (enum class)
//   FakeInString         declaration only inside a string literal

#include <string>

namespace one {
    namespace two {
        struct NestedStruct {
            int value;
            float ratio;
        };

        class NestedClass final {
        public:
            int x;
            NestedStruct make();
        };
    }

    union NestedUnion {
        int i;
        float f;
    };
}

// C++17 qualified namespace declaration
namespace one::four::five {
    class DeepClass {
    public:
        void go();
    };
}

namespace {
    struct AnonStruct {
        double d;
    };
}

template <typename T>
class Templated {
public:
    T data;
};

enum class Color { Red, Green, Blue };
enum class Prio : int { Low = 0, High = 10 };

const char* s = "class FakeInString { int x; }";

struct PlainStruct {
    int a;
};
