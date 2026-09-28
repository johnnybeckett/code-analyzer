using System;

namespace TestNamespace
{
    public class BaseClass
    {
        protected int baseField;

        public BaseClass()
        {
            baseField = 0;
        }

        public virtual void BaseMethod()
        {
            Console.WriteLine("Base method");
        }
    }
}