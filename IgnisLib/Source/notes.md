for full rendering:

	need to implement compute shaders


	needs the following descriptor set types modularized:
	storage buffer
	uniform buffer
	texture/sampler //should not be per frame
	constants		//passed per draw call

	descriptor set can only be created before creating the pipeline, no modification

	PushUniform1f,...
	PushUniformBuffer(struct of the data)
	PushTextureSampler(max texture)
	PushStorageBuffer(struct of the data, array size)

	Update for those

	make texture sampler customizable

	need to make a default pipeline with all its dependencies for the ui part

	
	make vertexdata customizable