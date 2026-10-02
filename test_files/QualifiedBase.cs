using System;

namespace Outer
{
    // A dotted (qualified) base: `MyNs.MyBase` must be captured as ONE base
    // and stored in the model's `::` form, not torn into `MyNs` and `MyBase`.
    public class Foo : MyNs.MyBase
    {
        public int Value;
    }
}
