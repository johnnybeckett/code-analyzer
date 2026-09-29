using System;

namespace NS.Sub
{
    public struct Point
    {
        public int X;
        public int Y;

        public void Move(int dx, int dy)
        {
            X += dx;
            Y += dy;
        }
    }
}
