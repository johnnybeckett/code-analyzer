using System;

namespace GenBase
{
    // A generic base: `Base<int>` must be captured as ONE base, not
    // split into `Base` and a spurious `int`.
    public class Foo : Base<int>
    {
        public int Value;
    }
}
