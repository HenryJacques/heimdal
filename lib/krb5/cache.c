#include "krb5_locl.h"

RCSID("$Id$");

/* XXX shouldn't be here */

void
krb5_free_ccache(krb5_context context,
		 krb5_ccache val)
{
    free(((krb5_fcache*)(val->data.data))->filename);
    krb5_data_free (&val->data);
    free(val);
}

extern krb5_cc_ops fcc_ops;

krb5_error_code
krb5_cc_register(krb5_context context, krb5_cc_ops *ops, int override)
{
    int i;
    if(context->cc_ops == NULL){
	context->num_ops = 4;
	context->cc_ops = calloc(context->num_ops, sizeof(*context->cc_ops));
    }
    for(i = 0; context->cc_ops[i].prefix && i < context->num_ops; i++){
	if(strcmp(context->cc_ops[i].prefix, ops->prefix) == 0){
	    if(override)
		free(context->cc_ops[i].prefix);
	    else
		return KRB5_CC_TYPE_EXISTS; /* XXX */
	}
    }
    if(i == context->num_ops){
	krb5_cc_ops *o = realloc(context->cc_ops, 
				 (context->num_ops + 4) * 
				 sizeof(*context->cc_ops));
	if(o == NULL)
	    return KRB5_CC_NOMEM;
	context->num_ops += 4;
	context->cc_ops = o;
	memset(context->cc_ops + i, 0, 
	       (context->num_ops - i) * sizeof(*context->cc_ops));
    }
    memcpy(&context->cc_ops[i], ops, sizeof(context->cc_ops[i]));
    context->cc_ops[i].prefix = strdup(ops->prefix);
    if(context->cc_ops[i].prefix == NULL)
	return KRB5_CC_NOMEM;
    
    return 0;
}


krb5_error_code
krb5_cc_resolve(krb5_context context,
		const char *residual,
		krb5_ccache *id)
{
    krb5_error_code ret;
    int i;

    if(context->cc_ops == NULL){
	ret = krb5_cc_register(context, &fcc_ops, 1);
	if(ret) return ret;
    }

    for(i = 0; context->cc_ops[i].prefix && i < context->num_ops; i++)
	if(strncmp(context->cc_ops[i].prefix, residual, 
		   strlen(context->cc_ops[i].prefix)) == 0){
	    krb5_ccache p;
	    p = malloc(sizeof(*p));
	    if(p == NULL)
		return KRB5_CC_NOMEM;
	    p->ops = &context->cc_ops[i];
	    *id = p;
	    ret =  p->ops->resolve(context, id, residual + 
				   strlen(p->ops->prefix) + 1);
	    if(ret) free(p);
	    return ret;
	}
    return KRB5_CC_UNKNOWN_TYPE;
}


#if 0
krb5_error_code
krb5_cc_gen_new(krb5_context context,
		krb5_cc_ops *ops,
		krb5_ccache *id)
{
}

krb5_error_code
krb5_cc_register(krb5_context context,
		 krb5_cc_ops *ops,
		 krb5_boolean override)
{
}
#endif

char*
krb5_cc_get_name(krb5_context context,
		 krb5_ccache id)
{
    return id->ops->get_name(context, id);
}

char*
krb5_cc_default_name(krb5_context context)
{
    static char name[1024];
    char *p;
    p = getenv("KRB5CCNAME");
    if(p)
	strcpy(name, p);
    else
	sprintf(name, "FILE:/tmp/krb5cc_%d", getuid());
    return name;
}




krb5_error_code
krb5_cc_default(krb5_context context,
		krb5_ccache *id)
{
    return krb5_cc_resolve(context, 
			   krb5_cc_default_name(context), 
			   id);
}

krb5_error_code
krb5_cc_initialize(krb5_context context,
		   krb5_ccache id,
		   krb5_principal primary_principal)
{
    return id->ops->init(context, id, primary_principal);
}


krb5_error_code
krb5_cc_destroy(krb5_context context,
		krb5_ccache id)
{
    return id->ops->destroy(context, id);
}

krb5_error_code
krb5_cc_close(krb5_context context,
	      krb5_ccache id)
{
    krb5_error_code ret;
    ret = id->ops->close(context, id);
    free(id->residual);
#if 0
    free(id);
#endif
    return ret;
}

krb5_error_code
krb5_cc_store_cred(krb5_context context,
		   krb5_ccache id,
		   krb5_creds *creds)
{
    return id->ops->store(context, id, creds);
}

krb5_error_code
krb5_cc_retrieve_cred(krb5_context context,
		      krb5_ccache id,
		      krb5_flags whichfields,
		      krb5_creds *mcreds,
		      krb5_creds *creds)
{
    krb5_error_code ret;
    krb5_cc_cursor cursor;
    krb5_cc_start_seq_get(context, id, &cursor);
    while((ret = krb5_cc_next_cred(context, id, creds, &cursor)) == 0){
	if(krb5_principal_compare(context, mcreds->server, creds->server)){
	    ret = 0;
	    break;
	}
    }
    krb5_cc_end_seq_get(context, id, &cursor);
    return ret;
}

krb5_error_code
krb5_cc_get_principal(krb5_context context,
		      krb5_ccache id,
		      krb5_principal *principal)
{
    return id->ops->get_princ(context, id, principal);
}

krb5_error_code
krb5_cc_start_seq_get (krb5_context context,
		       krb5_ccache id,
		       krb5_cc_cursor *cursor)
{
    return id->ops->get_first(context, id, cursor);
}

krb5_error_code
krb5_cc_next_cred (krb5_context context,
		   krb5_ccache id,
		   krb5_creds *creds,
		   krb5_cc_cursor *cursor)
{
    return id->ops->get_next(context, id, cursor, creds);
}

krb5_error_code
krb5_cc_end_seq_get (krb5_context context,
		     krb5_ccache id,
		     krb5_cc_cursor *cursor)
{
    return id->ops->end_get(context, id, cursor);
}

krb5_error_code
krb5_cc_remove_cred(krb5_context context,
		    krb5_ccache id,
		    krb5_flags which,
		    krb5_creds *cred)
{
    return id->ops->remove_cred(context, id, which, cred);
}

krb5_error_code
krb5_cc_set_flags(krb5_context context,
		  krb5_ccache id,
		  krb5_flags flags)
{
    return id->ops->set_flags(context, id, flags);
}
		    
