using System;

namespace MultiBase
{
    // Multiple comma-separated bases must stay distinct, and a generic base
    // with several type arguments (`B<C, D>`) must not be split on its
    // inner comma.
    public class Foo : A, B<C, D>
    {
        public int Value;
    }
}
