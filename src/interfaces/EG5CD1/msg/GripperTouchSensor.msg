# EG-5CD1 指尖触觉（寄存器 1200–1219，version=1）
# fingerResults["right"|"left"]：[0]法向力 [1]切向力方向 [2]切向力（int16）
# proximityResults["right"|"left"]：接近觉（uint32，寄存器 1208/1212 起各 4 字节）
std_msgs/Header header
int32 hand_id
int32 normal_force_right
int32 tangential_force_dir_right
uint32 proximity_right
int32 tangential_force_right
int32 normal_force_left
int32 tangential_force_dir_left
uint32 proximity_left
int32 tangential_force_left
uint32 touch_version
