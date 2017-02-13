//#include <sys/time.h>
#include "ruby.h"
#include "ruby/io.h"
//#include "ruby/encoding.h"
#include "mpi.h"

static int mp_buffer_size = 4098;
static int mp_initialized = 0;
static int mp_finalized = 0;
static VALUE sym_exception;
static VALUE eEAGAINWaitReadable;
static ID id_allocated_mpipe;

struct MPipe {
    int rank;
    char *send_buffer;
    char *recv_buffer;
    MPI_Request send_request;
    MPI_Request recv_request;
    int send_count;
    int recv_count;
    int recv_begin;
};

#define IS_MPIPE(obj) (rb_typeddata_is_kind_of((obj), &mp_data_type))
#define error_inval(msg) (rb_syserr_fail(EINVAL, msg))
//#define get_enc(ptr) ((ptr)->enc ? (ptr)->enc : rb_enc_get((ptr)->string))
#define get_enc(ptr) ((ptr)->enc)

static void
mp_finalize()
{
    if (mp_initialized && !mp_finalized) {
        mp_finalized = 1;
        MPI_Finalize();
    }
}

static VALUE
mp_mpi_init(int argc, VALUE *argv, VALUE klass)
{
    char **cargv;
    VALUE progname, mpipe_ary;
    int i, size;

    cargv = ALLOCA_N(char *, argc+1);
    progname = rb_gv_get("$0");
    cargv[0] = StringValueCStr(progname);

    for(i=0; i<argc; i++) {
        if (TYPE(argv[i]) == T_STRING) {
            cargv[i+1] = StringValueCStr(argv[i]);
        } else {
            rb_raise(rb_eArgError, "argument must be string");
        }
    }
    argc++;

    MPI_Init(&argc, &cargv);

    if (mp_initialized) {
        return Qnil;
    } else {
        mp_initialized = 1;
    }
    atexit(mp_finalize);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    mpipe_ary = rb_ary_new2(size);
    rb_ivar_set(klass, id_allocated_mpipe, mpipe_ary);

    return Qnil;
}

static VALUE
mp_mpi_abort(VALUE klass, VALUE rerror)
{
    int ierror;

    ierror = MPI_Abort(MPI_COMM_WORLD, NUM2INT(rerror));
    return INT2NUM(ierror);
}

static VALUE
mp_mpi_buffer_size(VALUE mod)
{
    return INT2NUM(mp_buffer_size);
}

static VALUE
mp_mpi_set_buffer_size(VALUE mod, VALUE size)
{
    if (mp_initialized) {
        rb_raise(rb_eStandardError,"buffer_size must be set before MPipe.init");
    }
    mp_buffer_size = NUM2INT(size);
    return size;
}

static VALUE
mp_comm_size(VALUE self)
{
    int size;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    return INT2NUM(size);
}

static VALUE
mp_comm_rank(VALUE self)
{
    int rank;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return INT2NUM(rank);
}

// ---------------------------------------------------------

static struct MPipe *
mp_alloc(void)
{
    struct MPipe *ptr = ALLOC(struct MPipe);
    ptr->rank = -1;
    ptr->send_buffer = 0;
    ptr->recv_buffer = 0;
    ptr->send_count = 0;
    ptr->recv_count = 0;
    ptr->recv_begin = 0;
    return ptr;
}

static void
mp_free(void *p)
{
    struct MPipe *ptr = p;

    //if (--ptr->count <= 0) {
    if (ptr) {

        if (ptr->send_count == -1) {
            MPI_Request_free(&ptr->send_request);
            ptr->send_count = 0;
        }
        if (ptr->recv_count == -1) {
            MPI_Request_free(&ptr->recv_request);
            ptr->recv_count = 0;
        }
        if (ptr->send_buffer) {
            xfree(ptr->send_buffer);
            ptr->send_buffer = 0;
        }
        if (ptr->recv_buffer) {
            xfree(ptr->recv_buffer);
            ptr->recv_buffer = 0;
        }

	xfree(ptr);
    }
}

static size_t
mp_memsize(const void *p)
{
    return sizeof(struct MPipe) + mp_buffer_size*2;
}

static const rb_data_type_t mp_data_type = {
    "mpipe",
    {
	0,
	mp_free,
	mp_memsize,
    },
    0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

#define check_mpipe(self) ((struct MPipe*)rb_check_typeddata((self), &mp_data_type))

static struct MPipe*
get_mpipe(VALUE self)
{
    struct MPipe *ptr = check_mpipe(rb_io_taint_check(self));

    if (!ptr) {
	rb_raise(rb_eIOError, "uninitialized stream");
    }
    return ptr;
}

#define MPipe(obj) get_mpipe(obj)

static VALUE
mp_s_allocate(VALUE klass)
{
    return TypedData_Wrap_Struct(klass, &mp_data_type, 0);
}

static VALUE
mp_init(struct MPipe *ptr, VALUE self, VALUE rank)
{
    int size;

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    ptr->rank = NUM2INT(rank);
    if (ptr->rank < 0 || ptr->rank >= size) {
        rb_raise(rb_eArgError,"Invalid rank has value %d but must be nonnegative and less than %d\n",ptr->rank,size);
    }
    ptr->send_buffer = malloc(mp_buffer_size);
    ptr->recv_buffer = malloc(mp_buffer_size);
    return self;
}

/*
 * call-seq: MPipe.new(string=""[, mode])
 *
 * Creates new MPipe instance from with _string_ and _mode_.
 */
static VALUE
mp_initialize(VALUE self, VALUE rank)
{
    struct MPipe *ptr = check_mpipe(self);

    if (!ptr) {
	DATA_PTR(self) = ptr = mp_alloc();
    }
    rb_call_super(0, 0);
    return mp_init(ptr, self, rank);
}

/* :nodoc: */
static VALUE
mp_s_new(VALUE klass, VALUE vrank)
{
    VALUE mpipe, mpipe_ary;
    int rank;

    rank = NUM2INT(vrank);
    mpipe_ary = rb_ivar_get(klass, id_allocated_mpipe);
    mpipe = rb_ary_entry(mpipe_ary, rank);
    if (NIL_P(mpipe)) {
        mpipe = rb_class_new_instance(1, &vrank, klass);
        rb_ary_store(mpipe_ary, rank, mpipe);
    }
    return mpipe;
}


/*
 * Returns +rank+.
 */
static VALUE
mp_rank(VALUE self)
{
    struct MPipe *ptr = MPipe(self);
    return INT2NUM(ptr->rank);
}

/*
 * Returns +false+.  Just for compatibility to IO.
 */
static VALUE
mp_false(VALUE self)
{
    MPipe(self);
    return Qfalse;
}

/*
 * Returns +nil+.  Just for compatibility to IO.
 */
static VALUE
mp_nil(VALUE self)
{
    MPipe(self);
    return Qnil;
}

#if 0
/*
 * Returns *pipempi* itself.  Just for compatibility to IO.
 */
static VALUE
mp_self(VALUE self)
{
    MPipe(self);
    return self;
}

/*
 * Returns 0.  Just for compatibility to IO.
 */
static VALUE
mp_0(VALUE self)
{
    MPipe(self);
    return INT2FIX(0);
}

/*
 * Returns the argument unchanged.  Just for compatibility to IO.
 */
static VALUE
mp_first(VALUE self, VALUE arg)
{
    MPipe(self);
    return arg;
}

/*
 * Raises NotImplementedError.
 */
static VALUE
mp_unimpl(int argc, VALUE *argv, VALUE self)
{
    MPipe(self);
    rb_notimplement();

    UNREACHABLE;
}
#endif

#define mp_close mp_nil
#define mp_flush mp_nil
#define mp_closed_p mp_false

static VALUE
mp_write(VALUE self, VALUE str)
{
    struct MPipe *ptr = MPipe(self);
    int istat;
    int pos, count;

    str = StringValue(str);
    pos = 0;

    while (pos < RSTRING_LEN(str)) {
        count = RSTRING_LEN(str) - pos;
        if (count > mp_buffer_size) {
            count = mp_buffer_size;
        }
        memcpy(ptr->send_buffer, RSTRING_PTR(str)+pos, count);

        istat = MPI_Send(ptr->send_buffer, count, MPI_CHAR, ptr->rank,
                         0, MPI_COMM_WORLD);
        if (istat != MPI_SUCCESS) {
            rb_raise(rb_eStandardError,"MPI_Send failed with status=%d\n",istat);
        }

        pos += count;
    }
    return INT2NUM(pos);
}


static int
copy_substr(struct MPipe *ptr, int max_len, VALUE outbuf, int outbuf_pos)
{
    int len;
    char *recv_buf = ptr->recv_buffer + ptr->recv_begin;

    len = max_len - outbuf_pos;
    if (len < ptr->recv_count) {
        ptr->recv_begin += len;
        ptr->recv_count -= len;
    } else {
        len = ptr->recv_count;
        ptr->recv_begin = 0;
        ptr->recv_count = 0;
    }
    MEMCPY(RSTRING_PTR(outbuf)+outbuf_pos, recv_buf, char, len);
    return len;
}

/*
 *  call-seq:
 *     ios.read(length [, outbuf])    -> string, outbuf, or nil
 *
 *  Reads <i>length</i> bytes from the I/O stream.
 *
 *  <i>length</i> must be a non-negative integer.
 */
static VALUE
mp_read(int argc, VALUE *argv, VALUE self)
{
    struct MPipe *ptr = MPipe(self);
    MPI_Status status;
    int istat;
    int outbuf_pos = 0;
    int max_len;
    VALUE maxlen = Qnil;
    VALUE outbuf = Qnil;

    rb_scan_args(argc, argv, "11", &maxlen, &outbuf);

    max_len = NUM2INT(maxlen);
    if (max_len < 0) {
        rb_raise(rb_eArgError, "negative length %d given", max_len);
    }

    if (NIL_P(outbuf)) {
        outbuf = rb_str_new(0, 0);
    }
    rb_str_resize(outbuf, max_len);

    if (ptr->recv_count == -1) { // requesting
        istat = MPI_Wait(&ptr->recv_request, &status);
        if (istat != MPI_SUCCESS) {
            rb_raise(rb_eStandardError,"MPI_Wait failed with status=%d",istat);
        }
        MPI_Get_count(&status, MPI_CHAR, &ptr->recv_count);
    }

    if (ptr->recv_count > 0) {
        outbuf_pos += copy_substr(ptr, max_len, outbuf, outbuf_pos);
    }

    while (outbuf_pos < max_len) {
        istat = MPI_Recv(ptr->recv_buffer, mp_buffer_size, MPI_CHAR, ptr->rank,
                         0, MPI_COMM_WORLD, &status);
        if (istat != MPI_SUCCESS) {
            rb_raise(rb_eStandardError,"MPI_Recv failed with status=%d\n",istat);
        }
        MPI_Get_count(&status, MPI_CHAR, &ptr->recv_count);

        outbuf_pos += copy_substr(ptr, max_len, outbuf, outbuf_pos);
    }

    return outbuf;
}


static int
call_test(struct MPipe *ptr)
{
    MPI_Status status;
    int complete = 0;
    int istat;

    // if (ptr->recv_count == -1)
    istat = MPI_Test(&ptr->recv_request, &complete, &status);
    if (istat != MPI_SUCCESS) {
        rb_raise(rb_eStandardError,"MPI_Test failed with status=%d",istat);
    }
    if (complete) {
        MPI_Get_count(&status, MPI_CHAR, &ptr->recv_count);
        return ptr->recv_count;
    }
    return 0;
}

static void
call_irecv(struct MPipe *ptr)
{
    int istat;

    if (ptr->recv_count == 0) {
        istat = MPI_Irecv(ptr->recv_buffer, mp_buffer_size, MPI_CHAR, ptr->rank,
                          0, MPI_COMM_WORLD, &ptr->recv_request);
        if (istat != MPI_SUCCESS) {
            rb_raise(rb_eStandardError,"MPI_Irecv failed with status=%d",istat);
        }
        ptr->recv_count = -1; // requesting
    }
}

/*
not requesting: recv_count=0
requesting: recv_count=-1
buffered: recv_count=n
*/

/*
 * call-seq:
 *   mpipe.read_nonblock(maxlen[, outbuf [, opts]])    -> string
 *
 * Similar to #read, but raises +EOFError+ at end of string unless the
 * +exception: false+ option is passed in.
 */
static VALUE
mp_read_nonblock(int argc, VALUE *argv, VALUE self)
{
    struct MPipe *ptr = MPipe(self);
    int outbuf_pos = 0;
    int max_len;
    VALUE maxlen = Qnil;
    VALUE outbuf = Qnil;
    VALUE opts = Qnil;

    rb_scan_args(argc, argv, "11:", &maxlen, &outbuf, &opts);

    max_len = NUM2INT(maxlen);
    if (max_len < 0) {
        rb_raise(rb_eArgError, "negative length %d given", max_len);
    }

    if (NIL_P(outbuf)) {
        outbuf = rb_str_new(0, 0);
    }
    rb_str_resize(outbuf, max_len);

    if (maxlen == 0) {
        return outbuf;
    }

    if (ptr->recv_count == -1) { // requesting
        if (call_test(ptr) == 0) {
            if (!NIL_P(opts) && rb_hash_lookup2(opts, sym_exception, Qundef) == Qfalse) {
                return Qnil;
            } else {
                rb_raise(eEAGAINWaitReadable,"MPI_Irecv would block");
            }
        }
    }
    if (ptr->recv_count > 0) {
        outbuf_pos += copy_substr(ptr, max_len, outbuf, outbuf_pos);
    }

    while (outbuf_pos < max_len) {
        call_irecv(ptr);
        if (call_test(ptr) == 0) {
            if (outbuf_pos > 0) {
                rb_str_resize(outbuf, outbuf_pos);
                return outbuf;
            } else
            if (!NIL_P(opts) && rb_hash_lookup2(opts, sym_exception, Qundef) == Qfalse) {
                return Qnil;
            } else {
                rb_raise(eEAGAINWaitReadable,"MPI_Irecv would block");
            }
        }
        outbuf_pos += copy_substr(ptr, max_len, outbuf, outbuf_pos);
    }
    return outbuf;
}


/*
 * call-seq:
 *   mpipe.test_recv    -> true or false
 *
 * calls MPI_Test
 */
static VALUE
mp_test_recv(VALUE self)
{
    struct MPipe *ptr = MPipe(self);

    call_irecv(ptr);
    if (ptr->recv_count == -1) { // requesting
        call_test(ptr);
    }
    if (ptr->recv_count > 0) {
        return Qtrue;
    } else {
        return Qfalse;
    }
}


void Init_mpipe()
{
    VALUE cMPipe, mComm;

    cMPipe = rb_define_class("MPipe", rb_cData);

    // MPI
    rb_define_module_function(cMPipe, "init", mp_mpi_init, -1);
    rb_define_module_function(cMPipe, "abort", mp_mpi_abort, 1);
    rb_define_module_function(cMPipe, "buffer_size", mp_mpi_buffer_size, 0);
    rb_define_module_function(cMPipe, "buffer_size=", mp_mpi_set_buffer_size, 1);

    rb_define_const(cMPipe, "MPI_VERSION", INT2NUM(MPI_VERSION));
    rb_define_const(cMPipe, "MPI_SUBVERSION", INT2NUM(MPI_SUBVERSION));
    rb_define_const(cMPipe, "SUCCESS", INT2NUM(MPI_SUCCESS));
    rb_define_const(cMPipe, "PROC_NULL", INT2NUM(MPI_PROC_NULL));

    // MPI::Comm
    mComm = rb_define_module_under(cMPipe, "Comm");
    rb_define_module_function(mComm, "rank", mp_comm_rank, 0);
    rb_define_module_function(mComm, "size", mp_comm_size, 0);

    //rb_include_module(cMPipe, rb_mEnumerable);
    rb_define_alloc_func(cMPipe, mp_s_allocate);
    rb_define_singleton_method(cMPipe, "new", mp_s_new, 1);
    rb_define_method(cMPipe, "initialize", mp_initialize, 1);

    rb_define_method(cMPipe, "write", mp_write, 1);
    rb_define_method(cMPipe, "write_nonblock", mp_write, 1);
    rb_define_method(cMPipe, "print", mp_write, 1);
    rb_define_method(cMPipe, "read", mp_read, -1);
    rb_define_method(cMPipe, "read_nonblock", mp_read_nonblock, -1);
    rb_define_method(cMPipe, "flush", mp_flush, 0);
    rb_define_method(cMPipe, "close", mp_close, 0);
    rb_define_method(cMPipe, "closed?", mp_closed_p, 0);
    rb_define_method(cMPipe, "test_recv", mp_test_recv, 0);
    rb_define_method(cMPipe, "rank", mp_rank, 0);

    sym_exception = ID2SYM(rb_intern("exception"));
    id_allocated_mpipe = rb_intern("allocated_mpipe");
    eEAGAINWaitReadable = rb_const_get(rb_cIO, rb_intern("EWOULDBLOCKWaitReadable"));
}
