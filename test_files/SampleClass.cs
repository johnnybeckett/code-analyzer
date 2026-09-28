using System;

namespace TestNamespace
{
    public class SampleClass : BaseClass
    {
        private int sampleField;
        protected string anotherField;
        public static string staticField;

        public SampleClass()
        {
            sampleField = 0;
        }

        public SampleClass(int value)
        {
            sampleField = value;
        }

        public int GetSampleField()
        {
            return sampleField;
        }

        public void SetSampleField(int value)
        {
            sampleField = value;
        }

        protected virtual void VirtualMethod()
        {
            Console.WriteLine("Virtual method called");
        }
    }
}