#version 330 core
/*----------------------------------------------------
   pad_lr.frag ― Replication-Padding (Left & Right)
   把 1 张 orgWidth×orgHeight 的纹理扩展为
   (orgWidth + 2·pad) × orgHeight，
   左 / 右边缘各用最左 / 最右 1 列像素进行复制填充。
----------------------------------------------------*/

in  vec2 TexCoords;        // 来自顶点着色器的插值坐标，范围 [0,1]，针对“加宽后的画布”
out vec4 FragColor;        // 输出颜色

uniform sampler2D inputTex; // 原始图像
uniform float orgWidth;     // 原图宽度（像素）
uniform float orgHeight;    // 原图高度（像素）
uniform float divergence;   // 预留基线百分比 (%)，决定 pad 大小

void main()
{
    /*------------------------------------------------
     * 1. 计算左右各要复制多少像素 (pad)
     *    pad = floor(orgWidth × divergence% + 2)
     *    +2  确保最少 2 像素并抵消浮点误差
     *----------------------------------------------*/
    float pad = floor(orgWidth * divergence * 0.01 + 2.0);

    /* 加宽后的总宽度 */
    float paddedWidth = orgWidth + 2.0 * pad;

    /*------------------------------------------------
     * 2. 把当前片元的归一化 UV 转成像素坐标
     *    x ∈ [0, paddedWidth)
     *    y ∈ [0, orgHeight)
     *----------------------------------------------*/
    float x = TexCoords.x * paddedWidth;
    float y = TexCoords.y * orgHeight;

    /*------------------------------------------------
     * 3. 映射回原图的像素坐标并夹到合法范围
     *
     *    x_src:
     *      ─ 左 pad  区 (x < pad)        → clamp 到 0
     *      ─ 中心区   (pad ≤ x < pad+W) → 0 … W-1
     *      ─ 右 pad  区 (x ≥ pad+W)     → clamp 到 W-1
     *
     *    y_src 只需要保证不越界
     *----------------------------------------------*/
    float x_src = clamp(x - pad, 0.0, orgWidth  - 1.0);
    float y_src = clamp(y,       0.0, orgHeight - 1.0);

    /*------------------------------------------------
     * 4. 计算采样用的归一化坐标 sampleUV
     *
     *    +0.5 把整数像素索引移到“像素中心”，
     *    避免 GL_LINEAR 双线性过滤时混入相邻列 / 行。
     *----------------------------------------------*/
    vec2 sampleUV = vec2((x_src + 0.5) / orgWidth,
                         (y_src + 0.5) / orgHeight);

    /* 5. 采样并输出 */
    FragColor = texture(inputTex, sampleUV);
}