struct list_head {
    struct list_head *next, *prev;
};

struct device_manager_data {
    char                         device_name[TSENS_NAME_MAX];
    union device_request         active_req;
    struct list_head             client_list;
    struct list_head             dev_ptr;
    struct mutex                 clnt_lock;
    int (*request_validate)(struct device_clnt_data *, union device_request *, enum device_req_type);
    int (*update)(struct device_manager_data *);
    void                         *data;
};

struct device_clnt_data {
    struct device_manager_data   *dev_mgr;
    bool                         req_active;
    union device_request         request;
    struct list_head             clnt_ptr;
    void (*callback)(struct device_clnt_data *, union device_request *req, void *);
    void                         *usr_data;
};

struct devmgr_devices {
    struct device_manager_data *hotplug_dev;
    struct device_manager_data *cpufreq_dev[NR_CPUS];
};

struct device_clnt_data *clnt = NULL;
static struct devmgr_devices *devices;

list_for_each_entry(clnt, &devices->hotplug_dev->client_list, clnt_ptr) {
    if (clnt->callback) {
        clnt->callback(clnt, &req, clnt->usr_data);
    }
}

#define list_for_each_entry(pos, head, member)              \
    for (pos = list_first_entry(head, typeof(*pos), member);    \
         &pos->member != (head);                    \
         pos = list_next_entry(pos, member))

#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

#define offsetof(TYPE, MEMBER)  ((size_t)&((TYPE *)0)->MEMBER)








