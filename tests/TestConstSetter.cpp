class ConstSetter {
private:
   mutable int value;
public:
   ConstSetter(int val) { value = val; }
   void setVal(int new_val) const { value = new_val; }
};
