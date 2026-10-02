using System;

namespace GenClass
{
    // A generic class: `Foo<T>` must parse (the old name pattern was only
    // `(\w+)`, so the `<T>` made the whole declaration fail to match).
    public class Foo<T> : Base
    {
        public int Value;
    }
}
