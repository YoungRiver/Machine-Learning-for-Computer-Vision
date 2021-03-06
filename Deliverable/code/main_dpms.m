%% Maximum Likelihood parameter estimation for pairwise terms
for im_id=1:1000,
    [input_image,points] = load_im(im_id,1,1);
    center = points(:,5);
    offsets(:,:,im_id) = points(:,1:4) - center*ones(1,4);
end

for pt = [1:4],
    mn{pt} = mean(squeeze(offsets(:,pt,:)),2);
    sg{pt} = sqrt(diag(cov(squeeze(offsets(:,pt,:))')));
end

%% take a look at the data
strs = {'left eye','right eye','left mouth','right mouth','nose'};

clrs = {'r','g','b','k','y'};
figure,
for pt = [1:4],
    scatter(squeeze(offsets(1,pt,:)),squeeze(offsets(2,pt,:)),clrs{pt},'filled'); hold on,
    text(mn{pt}(1),mn{pt}(2),strs{pt},'fontsize',30)
end
axis ij; axis equal;

%% compute unary terms
t= load('svm_linear');
for part = 1:5
    weights_unary(part,:) = t.svm_linear{part}.weight;
end

im_id         = 1;  % 3, 231, 507
[input_image] = load_im(im_id,1,1);
[feats,~,idxs]= get_features(input_image,'SIFT');
responses     = weights_unary*feats;
[sv,sh]       = size(input_image);
for pt_ind = [1:5],
    score       = -10*ones(sv,sh);
    score(idxs) = responses(pt_ind,:);
    score_part{pt_ind} = score;
end

figure
subplot(2,3,1); imshow(input_image);
for pt_ind = [1:5],
    subplot(2,3,1+pt_ind);
    imshow(score_part{pt_ind},[-2,2]);
    title([strs{pt_ind},' with SVM-Linear - SIFT']);
end

%% dt- potential:  def(1) h^2 + def(2) h + def(3) * v^2 + def(4) *v
%% gaussian potential:   (h - mh)^2/(2*sch^2) + (v-mv)^2/(2*scv^2)
for pt = [1:4]
    sch = sg{pt}(1);
    scv = sg{pt}(2);
    mh  = -mn{pt}(1);
    mv  = -mn{pt}(2);
    
    def(1) = 1/(2*sch^2);
    def(2) = -2*mh/(2*sch^2);
    def(3) = 1/(2*scv^2);
    def(4) = -2*mv/(2*scv^2);
    
    [mess{pt},ix{pt},iy{pt}] = dt(squeeze(score_part{pt}),def(1),def(2),def(3),def(4));
    offset =  mh^2/(2*sch^2) + mv^2/(2*scv^2);
    mess{pt} = mess{pt} - offset;
end

belief_nose = squeeze(score_part{5});

parts = {'left eye','right eye','left mouth','right mouth','nose'};
figure,
for pt = [1:4],
    subplot(2,2,pt);
    imshow(mess{pt},[-2,2]); title(['\mu_{',parts{pt},'-> nose}(X)'],'fontsize',20);
    belief_nose = belief_nose + mess{pt};
end

figure,
subplot(1,2,1);
imshow(input_image);
subplot(1,2,2);
imagesc(max(belief_nose,-10));
axis image;

%% Home-made max-product algorithm

my_mess = cell(1,4);
row_points = zeros(sv,sh); % value = row number
col_points = zeros(sv,sh); % value = column number
for i = 1:sv
    for j=1:sh
        row_points(i,j) = i;
        col_points(i,j) = j;
    end
end

for pt = [1:4]
    pt % see at which part we are in the computation
    sch = sg{pt}(1);
    scv = sg{pt}(2);
    mh  = mn{pt}(1);
    mv  = mn{pt}(2);
    
    my_mess{pt} = zeros(sv,sh);
    for Xr_1 = 1:sv
        Xr_1  % see at which row we are in the computation
        for Xr_2 = 1:sh
            to_max = score_part{pt}...
                + pairwise(row_points, col_points, Xr_1, Xr_2, sch, scv, mh, mv);
            my_mess{pt}(Xr_1, Xr_2) = max(to_max(:));
        end
    end
end

my_belief_nose = squeeze(score_part{5});

figure,
for pt = [1:4],
    subplot(2,2,pt);
    imshow(my_mess{pt},[-8,-4]); title(['\mu_{',parts{pt},'-> nose}(X)'],'fontsize',20);
    my_belief_nose = my_belief_nose + my_mess{pt};
end

figure,
subplot(1,2,1);
imshow(input_image);
subplot(1,2,2);
imagesc(max(my_belief_nose,max(my_belief_nose(:))-6));
axis image;

%% Root-to-leaves message passing

mess_to_leaves = cell(1,4);
for pt = [1:4]
    sch = sg{pt}(1);
    scv = sg{pt}(2);
    mh  = mn{pt}(1);
    mv  = mn{pt}(2);
    
    def(1) = 1/(2*sch^2);
    def(2) = -2*mh/(2*sch^2);
    def(3) = 1/(2*scv^2);
    def(4) = -2*mv/(2*scv^2);
    
    msg_sum = zeros(sv,sh);
    for i = 1:4
        if i~=pt
            msg_sum = msg_sum + mess{i};
        end
    end
    
    [mess_to_leaves{pt},ix_leaves{pt},iy_leaves{pt}] = dt(squeeze(score_part{5}+msg_sum),def(1),def(2),def(3),def(4));
    offset =  mh^2/(2*sch^2) + mv^2/(2*scv^2);
    mess_to_leaves{pt} = mess_to_leaves{pt} - offset;
end

figure,
for pt = [1:4],
    subplot(2,2,pt);
    imshow(mess_to_leaves{pt},[max(mess_to_leaves{pt}(:))-5,max(mess_to_leaves{pt}(:))]); title(['\mu_{nose->',parts{pt},'}(X)'],'fontsize',20);
end

%% show ground-truth bounding box. 
%% You will need to adapt this code to make it show your bounding box proposals
addpath('util/');
[input_image,points] = load_im(im_id,1,1);

figure,
min_x = min(points(1,:));
max_x = max(points(1,:));
min_y = min(points(2,:));
max_y = max(points(2,:));
score = 1;
bbox  = [min_x,min_y,max_x,max_y,score];
showboxes(input_image,bbox);

%% Home-made box
[input_image,points] = load_im(im_id,1,1);

points = zeros(2,4);
for pt = 1:4
    [max_val, max_idx] = max(mess_to_leaves{pt}(:));
    points(1,pt) = ceil(max_idx/size(mess_to_leaves{pt},1));
    points(2,pt) = mod(max_idx,size(mess_to_leaves{pt},1));
end

figure,
min_x = min(points(1,:));
max_x = max(points(1,:));
min_y = min(points(2,:));
max_y = max(points(2,:));
score = 1;
bbox  = [min_x,min_y,max_x,max_y,score];
showboxes(input_image,bbox);