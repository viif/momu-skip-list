# momu-skip-list

C++ 实现的简单跳表。跳表因其简单性与高效率，被广泛应用于工业界项目中，如 Redis、LevelDB 等。

## 接口

- insert_element：插入数据
- delete_element：删除数据
- search_element：查找数据
- size：获取跳表元素个数

## 性能

每秒可处理写请求数 (QPS): 24.4w

每秒可处理读请求数 (QPS): 18.4w
