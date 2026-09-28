class SampleClass(BaseClass):
    def __init__(self):
        self.field = 0

    def sample_method(self):
        return self.field

class AnotherClass(SampleClass, BaseClass):
    pass