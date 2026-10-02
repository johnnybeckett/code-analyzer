using System;

namespace Multi
{
    // A base list spread across multiple lines must still be captured fully
    // (the old regex's `.` could not cross newlines, so this failed to parse).
    public class Foo :
        BaseOne,
        BaseTwo
    {
        public int Value;
    }
}
